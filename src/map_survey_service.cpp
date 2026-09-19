#include "map_survey_service.h"
#include "input_direction.h"

#include "async_log.h"
#include "map_survey.h"
#include "native_entities.h"
#include "native_world.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <D2RLPlugin/input.h>
#include <D2RLPlugin/lifecycle_events.h>

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace D2RAccess::MapSurveyService {

namespace {

using Kind = D2RL::Lifecycle::GameplayEventKind;

// Wide enough to cover a level's walkable body from wherever the player is
// standing. The grid is only built when a key is pressed, so the cost is paid
// per question, not per update.
constexpr int SurveyRadius = 200;
// The registry is fed from the ordinary update, which must stay cheap.
constexpr uint64_t ObserveIntervalMs = 500;

struct Lifecycle {
	uint32_t player = 0;
	uint64_t session = 0;
	int      level = 0;
	bool     ready = false;
};

std::mutex                 g_mutex;
Lifecycle                  g_state;
MapSurvey::Registry        g_registry;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService*  g_input = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
bool                       g_supported = false;
uint64_t                   g_nextObserve = 0;

// What the player is currently pointed at. Kept by identity, not index, so a
// rescan that adds or removes objects does not silently move the selection.
EntityRadar::Category g_category = EntityRadar::Category::Unknown;
uint32_t              g_selected = 0;

enum class Request : int { None, Repeat, PreviousObject, NextObject, PreviousCategory, NextCategory };
std::atomic<Request> g_request{ Request::None };
bool                 g_pathfinder = false; // L3: right stick drives the catalogue

bool Playing(const Lifecycle& state) { return state.ready && state.level != 0; }

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) g_state.level = event->currentValue;
}

const char* CategoryName(EntityRadar::Category category) {
	const auto& names = Strings::Current().surveyCategories;
	// MapSurvey::Categories only ever yields the ten real categories, in the
	// same order the table lists them.
	switch (category) {
	case EntityRadar::Category::Exit:         return names[0];
	case EntityRadar::Category::Portal:       return names[1];
	case EntityRadar::Category::Waypoint:     return names[2];
	case EntityRadar::Category::Npc:          return names[3];
	case EntityRadar::Category::Stash:        return names[4];
	case EntityRadar::Category::Enemy:        return names[5];
	case EntityRadar::Category::Treasure:     return names[6];
	case EntityRadar::Category::Shrine:       return names[7];
	case EntityRadar::Category::Interactable: return names[8];
	case EntityRadar::Category::Item:         return names[9];
	default:                                  return names[8];
	}
}

std::string Format(const char* format, auto... args) {
	char buffer[256]{};
	std::snprintf(buffer, sizeof(buffer), format, args...);
	return buffer;
}

std::string Describe(const MapSurvey::Landmark& landmark) {
	const Strings::Table& strings = Strings::Current();
	const char* direction = strings.radarDirections[static_cast<size_t>(landmark.direction)];
	return landmark.reachable
		? Format(strings.surveySteps, landmark.entity.name.c_str(), direction, landmark.steps)
		: Format(strings.surveyNoRoute, landmark.entity.name.c_str(), direction);
}

// Reads the world fresh: the answer must reflect where the player is standing
// now, not where they were when the registry was last fed.
bool CurrentSurvey(std::vector<MapSurvey::Landmark>& landmarks) {
	Lifecycle state;
	{
		std::lock_guard lock(g_mutex);
		state = g_state;
	}
	if (!g_supported || !Playing(state)) return false;
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return false;
	const Navigation::Grid grid = Navigation::Build(world, SurveyRadius);
	std::vector<EntityRadar::Entity> entities;
	if (NativeEntities::Read(state.player, state.session, entities, true)) {
		g_registry.Observe(state.level, entities, grid);
	}
	landmarks = MapSurvey::Survey(world, grid, g_registry.Entities());
	return true;
}

void Answer(const D2RL::PluginContext* context, Request request) {
	const Strings::Table& strings = Strings::Current();
	std::vector<MapSurvey::Landmark> landmarks;
	if (!CurrentSurvey(landmarks) || landmarks.empty()) {
		ScreenReader::Speak(strings.surveyNothing, true);
		return;
	}

	if (request == Request::PreviousCategory || request == Request::NextCategory) {
		g_category = MapSurvey::StepCategory(landmarks, g_category, request == Request::NextCategory ? 1 : -1);
		g_selected = 0;
	} else if (g_category == EntityRadar::Category::Unknown) {
		g_category = MapSurvey::StepCategory(landmarks, g_category, 0);
	}

	const std::vector<MapSurvey::Landmark> members = MapSurvey::OfCategory(landmarks, g_category);
	if (members.empty()) {
		ScreenReader::Speak(Format(strings.surveyEmpty, CategoryName(g_category)), true);
		return;
	}

	size_t index = 0;
	for (size_t i = 0; i < members.size(); ++i) {
		if (members[i].entity.id == g_selected) { index = i; break; }
	}
	if (request == Request::NextObject) index = (index + 1) % members.size();
	else if (request == Request::PreviousObject) index = (index + members.size() - 1) % members.size();
	g_selected = members[index].entity.id;

	// Category and position first when the category changed, so the player
	// knows where they are in the list before hearing the object itself.
	std::string text;
	if (request == Request::PreviousCategory || request == Request::NextCategory) {
		text = Format(strings.surveyCategory, CategoryName(g_category), static_cast<int>(members.size()));
		text += ". ";
	}
	text += Describe(members[index]);
	AsyncLog::InfoF(context, "[survey] category=%s %zu/%zu -> %s",
		CategoryName(g_category), index + 1, members.size(), text.c_str());
	ScreenReader::Speak(text, true);
}

auto __cdecl OnAction(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* userData) noexcept
	-> D2RL::Input::ActionResult {
	if (event == nullptr || event->kind != D2RL::Input::ActionEventKind::Pressed) {
		return D2RL::Input::ActionResult::Ignored;
	}
	g_request.store(static_cast<Request>(reinterpret_cast<intptr_t>(userData)));
	return D2RL::Input::ActionResult::Handled;
}

}

// The long-range pathfinder is a MODE, not a key. L3 toggles it, and while it
// is on the right stick drives the catalogue instead of the entity scan:
// left/right through the objects in a category, up/down through categories.
// TakeRightStickPush already resolves one dominant axis per push, so a
// slightly diagonal push cannot change the category and the object at once.
bool Active() { return g_pathfinder; }

void PollPathfinder(const D2RL::PluginContext* context) {
	if (InputDirection::TakeLeftStickClick()) {
		g_pathfinder = !g_pathfinder;
		AsyncLog::InfoF(context, "[survey] long-range pathfinder %s", g_pathfinder ? "on" : "off");
		ScreenReader::Speak(g_pathfinder ? Strings::Current().pathfinderEnabled : Strings::Current().pathfinderDisabled, true);
	}
	if (!g_pathfinder) return;
	switch (InputDirection::TakeRightStickPush()) {
	case InputDirection::StickPush::Left:  g_request.store(Request::PreviousObject); break;
	case InputDirection::StickPush::Right: g_request.store(Request::NextObject); break;
	case InputDirection::StickPush::Up:    g_request.store(Request::PreviousCategory); break;
	case InputDirection::StickPush::Down:  g_request.store(Request::NextCategory); break;
	default: break;
	}
}

void CycleObject(int delta) {
	g_request.store(delta >= 0 ? Request::NextObject : Request::PreviousObject);
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context   = context;
	g_supported = NativeWorld::Initialize(context) && NativeEntities::Initialize(context);
	if (!g_supported) return false;

	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize,
			.kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) return false;
		g_listeners.push_back(handle);
	}


	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		struct Entry { const char* id; const char* label; D2RL::Input::Key key; Request request; };
		for (const Entry& entry : {
			Entry{"survey-repeat", "Repeat the current map object", D2RL::Input::Key::Backslash, Request::Repeat},
			Entry{"survey-category-previous", "Previous map category", D2RL::Input::Key::Comma, Request::PreviousCategory},
			Entry{"survey-category-next", "Next map category", D2RL::Input::Key::Period, Request::NextCategory}}) {
			const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = entry.id, .displayName = entry.label, .category = "Accessibility",
				.defaultPrimary = {entry.key, D2RL::Input::Modifier::None}, .callback = OnAction,
				.userData = reinterpret_cast<void*>(static_cast<intptr_t>(entry.request))};
			D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
			if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) {
				g_actions.push_back(handle);
			}
		}
	}
	return true;
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	if (!g_supported) return;

	PollPathfinder(context);

	if (const Request request = g_request.exchange(Request::None); request != Request::None && !uiOwnsSpeech) {
		Answer(context, request);
		return;
	}

	// Keep the catalogue fed while walking, so an object seen once is still
	// known after it leaves the window. Cheap enough at two hertz; the grid
	// here is the small one the registry needs to tell absence from ignorance.
	const uint64_t now = GetTickCount64();
	if (now < g_nextObserve) return;
	g_nextObserve = now + ObserveIntervalMs;

	Lifecycle state;
	{
		std::lock_guard lock(g_mutex);
		state = g_state;
	}
	if (!Playing(state)) return;
	Radar::World world;
	std::vector<EntityRadar::Entity> entities;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)
		|| !NativeEntities::Read(state.player, state.session, entities, true)) {
		return;
	}
	g_registry.Observe(state.level, entities, Navigation::Build(world, SurveyRadius));
}

void Shutdown() {
	if (g_context && g_input) {
		for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	}
	if (g_context && g_lifecycle) {
		for (const auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	}
	g_listeners.clear();
	g_actions.clear();
	g_registry.Clear();
}

}
