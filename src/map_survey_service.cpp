#include "map_survey_service.h"
#include "input_direction.h"

#include "async_log.h"
#include "beacon_service.h"
#include "map_survey.h"
#include "layout_check.h"
#include "native_drlg.h"
#include "native_entities.h"
#include "level_areas.h"
#include "native_level_areas.h"
#include "native_automap.h"
#include "native_rooms.h"
#include "drlg_layout.h"
#include "room_catalogue.h"
#include "native_world.h"
#include "navigation_mode.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "terrain_service.h"

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

// As wide as the graph can be built, which is as wide as the game keeps
// collision around the player. Taken from the cap rather than written out: a
// larger number does not widen the catalogue, it empties it -- Build returns
// an empty grid above the cap, and every object then reads as "no route".
constexpr int SurveyRadius = Navigation::MaxRadius;
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
MapSurvey::Memory          g_registry;
RoomCatalogue::Registry    g_rooms;
// The level's logical rooms do not change while the level instance lives, so
// the native walk happens once per instance rather than on every observation.
std::vector<NativeRooms::Room> g_roomList;
int                        g_roomLevel = 0;
uint64_t                   g_roomSession = 0;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService*  g_input = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
bool                       g_supported = false;
uint64_t                   g_nextObserve = 0;
// The level the area list was last logged for; diagnostics only.
int                        g_areaLevel = 0;
uint64_t                   g_areaSession = 0;

// What the player is currently pointed at. Kept by identity, not index, so a
// rescan that adds or removes objects does not silently move the selection.
EntityRadar::Category g_category = EntityRadar::Category::Unknown;
uint32_t              g_selected = 0;

enum class Request : int { None, Repeat, PreviousObject, NextObject, PreviousCategory, NextCategory };
std::atomic<Request> g_request{ Request::None };
bool                 g_fullAssistance = false;

bool Playing(const Lifecycle& state) { return state.ready && state.level != 0; }

// The game the registry's entries were seen in. The registry clears itself
// when the level changes, but a town is the same level in every game, and a
// new game hands every NPC a new unit id -- so the last game's Akara was never
// matched again, never forgotten either (she stood some 1800 subtiles outside
// the new map), and was listed beside the real one. MEASURED 2026-09-21:
// "Akara, 6 steps, to the right" and "Akara, 1835 steps, to the left".
uint64_t g_registrySession = 0;

void ForgetOtherGames(const Lifecycle& state) {
	if (state.session == g_registrySession) return;
	AsyncLog::InfoF(g_context, "[survey] game session %llu: catalogue cleared, %zu entries from the last game dropped",
		static_cast<unsigned long long>(state.session), g_registry.Entities().size());
	g_registry.Clear();
	g_registrySession = state.session;
	g_selected = 0;
}

void ApplyMode() {
	const bool fullAssistance = NavigationMode::FullAssistance();
	if (fullAssistance == g_fullAssistance) return;
	g_fullAssistance = fullAssistance;
	g_registry.Clear();
	g_selected = 0;
	g_category = EntityRadar::Category::Unknown;
}

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
	// MapSurvey::Categories only ever yields real categories; the table is
	// indexed by this switch, so a new category appends to the table.
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
	case EntityRadar::Category::Room:         return names[10];
	case EntityRadar::Category::Player:       return names[11];
	case EntityRadar::Category::UnexploredRoom: return names[12];
	default:                                  return names[8];
	}
}

std::string Format(const char* format, auto... args) {
	char buffer[256]{};
	std::snprintf(buffer, sizeof(buffer), format, args...);
	return buffer;
}

// Rooms of the level being played. Empty when they cannot be read, which
// simply leaves the rooms category out rather than failing the catalogue.
const std::vector<NativeRooms::Room>& CurrentRooms(int level, uint64_t session) {
	if (g_roomLevel != level || g_roomSession != session) {
		if (!NativeRooms::Read(NativeWorld::PlayerRoom(), g_roomList)) g_roomList.clear();
		g_roomLevel = level;
		g_roomSession = session;
	}
	return g_roomList;
}

std::string Describe(const MapSurvey::Landmark& landmark) {
	const Strings::Table& strings = Strings::Current();
	// The shared words for a bearing and a distance, so every readout names
	// the same thing the same way: "ahead" rather than "Up", and steps rather
	// than a unit only this readout uses. The number here is
	// a real step count from the navigation grid, which is what made "metres"
	// the wrong word for it.
	const char* direction = strings.scanDirections[static_cast<size_t>(landmark.direction)];
	// Nothing here is out of reach. The grid only covers the collision window
	// the game keeps around the player, so a landmark it cannot route to is
	// almost always one the catalogue can see and the grid cannot -- the
	// catalogue is the level-wide reader precisely for that. Quote the
	// straight line, which MapSurvey::Survey fills in from world positions for
	// every landmark whether the grid saw it or not.
	//
	// Say it plainly: a straight line is not a walk, so this understates a
	// target on the far side of a wall. That is the trade the readout wants --
	// a direction and a rough distance can be acted on, "no route" cannot.
	// Only a landmark standing on the player has no distance to quote at all.
	if (!g_fullAssistance) {
		const std::string distance = EntityRadar::DistancePhrase(landmark.straightLine, EntityRadar::Settings{}, strings);
		return Format(strings.surveyOrientation, landmark.entity.name.c_str(), distance.c_str(), direction);
	}
	const double reach = landmark.reachable ? static_cast<double>(landmark.steps) : landmark.straightLine;
	if (!landmark.reachable && landmark.straightLine <= 0) {
		return Format(strings.surveyBearing, landmark.entity.name.c_str(), direction);
	}
	// Spoken like the scan's distance. For a reachable landmark it is the
	// WALKING distance -- the steps the game's own graph needs to get there --
	// and a wall between the two makes the numbers differ.
	const std::string distance = EntityRadar::DistancePhrase(reach, EntityRadar::Settings{}, strings);
	return Format(strings.surveySteps, landmark.entity.name.c_str(), distance.c_str(), direction);
}

// What the act has built around this level, once per level. The exits
// category is fed from this: a town has no warp of its own, so the only way
// out it can offer is the area next door. If a neighbour is missing from the
// list there is nothing to point at, and this line says which of the two
// happened rather than leaving an empty category to be guessed about.
void LogAreas(const D2RL::PluginContext* context, const Lifecycle& state) {
	if (state.level == g_areaLevel && state.session == g_areaSession) return;
	g_areaLevel = state.level; g_areaSession = state.session;
	std::vector<NativeLevelAreas::Area> areas;
	if (!NativeDrlg::Available() || !NativeLevelAreas::Read(NativeWorld::PlayerRoom(), areas)) {
		AsyncLog::Info(context, "[areas] unavailable");
		return;
	}
	std::string built;
	for (const auto& area : areas) {
		char entry[80]{};
		std::snprintf(entry, sizeof(entry), " %u@%d,%d %dx%d/%zur", area.level,
			area.tileX, area.tileY, area.tileWidth, area.tileHeight, area.rooms);
		built += entry;
	}
	AsyncLog::InfoF(context, "[areas] level %d, %zu built:%s", state.level, areas.size(), built.c_str());
	for (const auto& crossing : LevelAreas::Neighbours(areas, static_cast<uint32_t>(state.level)))
		AsyncLog::InfoF(context, "[areas] crossing to level %u at %.0f,%.0f gap=%d tiles",
			crossing.level, crossing.position.x, crossing.position.y, crossing.gapTiles);
}

using MapSurvey::Scene;

bool Collect(const Lifecycle& state, Scene& scene) {
	std::vector<EntityRadar::Entity> entities;
	if (!NativeEntities::Read(state.player, state.session, entities, true, true)) return false;
	scene = MapSurvey::Split(std::move(entities));
	return true;
}

// Exploration admits what came into view and, while the automap can be read,
// anything in a room it has drawn; full assistance everything loaded.
std::vector<EntityRadar::Entity> Landmarks(const Lifecycle& state, const Radar::World& world, const Scene& scene) {
	if (g_fullAssistance) return scene.landmarks;
	if (const auto charted = NativeAutomap::RevealedRooms(CurrentRooms(state.level, state.session)))
		return MapSurvey::Charted(world, scene.landmarks, *charted);
	return MapSurvey::Discoverable(world, scene.landmarks);
}

// Enemies are never charted: in range and in sight right now, or all loaded with full assistance.
std::vector<EntityRadar::Entity> Enemies(const Radar::World& world, const Scene& scene) {
	return g_fullAssistance ? scene.enemies : MapSurvey::Discoverable(world, scene.enemies);
}

void Observe(const Lifecycle& state, const Radar::World& world, const Navigation::Grid& grid, const Scene& scene) {
	g_registry.Observe(state.level, Landmarks(state, world, scene), grid, !g_fullAssistance, {}, &scene.landmarks);
}

// Reads the world fresh: the answer must reflect where the player is standing
// now, not where they were when the registry was last fed.
bool CurrentSurvey(std::vector<MapSurvey::Landmark>& landmarks, int& level) {
	ApplyMode();
	Lifecycle state;
	{
		std::lock_guard lock(g_mutex);
		state = g_state;
	}
	level = state.level;
	if (!g_supported || !Playing(state)) return false;
	ForgetOtherGames(state);
	g_registry.Select(state.level, !g_fullAssistance);
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return false;
	const Navigation::Grid grid = Navigation::Build(world, SurveyRadius);
	Scene scene;
	if (Collect(state, scene)) Observe(state, world, grid, scene);
	g_rooms.Observe(state.level, NativeDrlg::MapSeed(NativeWorld::PlayerRoom(), state.level),
		CurrentRooms(state.level, state.session), world.player);
	std::vector<EntityRadar::Entity> catalogued = g_registry.Entities();
	const auto enemies = Enemies(world, scene);
	catalogued.insert(catalogued.end(), enemies.begin(), enemies.end());
	// Other players are listed whenever loaded, in sight or not: finding a teammate is the point.
	catalogued.insert(catalogued.end(), scene.players.begin(), scene.players.end());
	const Strings::Table& strings = Strings::Current();
	// Rooms are listed whether or not the game has them loaded. Exploration keeps
	// the rooms stood in and the unexplored ones at the edge of the known map.
	auto entries = g_rooms.Entries();
	if (!g_fullAssistance) {
		const auto& levelRooms = CurrentRooms(state.level, state.session);
		entries = RoomCatalogue::Exploration(entries, TerrainService::VisitedRooms(state.level, levelRooms),
			TerrainService::ExploredRooms(state.level, levelRooms, world.player));
	}
	const auto rooms = RoomCatalogue::Entities(entries, strings.surveyRoom, strings.surveyRoomUnexplored);
	catalogued.insert(catalogued.end(), rooms.begin(), rooms.end());
	landmarks = MapSurvey::Survey(world, grid, catalogued, g_fullAssistance);
	return true;
}

void Answer(const D2RL::PluginContext* context, Request request) {
	const Strings::Table& strings = Strings::Current();
	std::vector<MapSurvey::Landmark> landmarks;
	int level = 0;
	if (!CurrentSurvey(landmarks, level) || landmarks.empty()) {
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
	// Hand the selection to the beacon, so the right-stick click leads to what
	// the scanner just named. Every answer re-offers, including a plain repeat,
	// so the offer is always the thing the player heard most recently.
	BeaconService::Offer(members[index].entity, level, GetTickCount64());

	// The category name first when the category changed, so the player knows
	// which list they are in before hearing the object itself. The name only:
	// how many it holds is a count, and counts are not spoken.
	std::string text;
	if (request == Request::PreviousCategory || request == Request::NextCategory) {
		text = CategoryName(g_category);
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

// The right stick always drives the scanner during play: left/right through
// the objects in a category, up/down through categories. TakeRightStickPush
// resolves one dominant axis per push, so a slightly diagonal push cannot
// change the category and the object at once. In a menu the stick belongs to
// the menu (the user, 2026-09-21: "pathfinder should only work when menu is closed").
void PollStick() {
	// Peek before the gate: it walks the panel tree, and the stick is idle on
	// all but a few of these ticks. A push left unclaimed stays pending for the
	// reader that does want it -- in a menu, the menu's own.
	if (InputDirection::PeekRightStickPush() == InputDirection::StickPush::None || !RadarUi::InPlay()) return;
	switch (InputDirection::TakeRightStickPush("scanner")) {
	case InputDirection::StickPush::Left:  g_request.store(Request::PreviousObject); break;
	case InputDirection::StickPush::Right: g_request.store(Request::NextObject); break;
	case InputDirection::StickPush::Up:    g_request.store(Request::PreviousCategory); break;
	case InputDirection::StickPush::Down:  g_request.store(Request::NextCategory); break;
	default: break;
	}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context   = context;
	g_supported = NativeWorld::Initialize(context) && NativeEntities::Initialize(context);
	if (!g_supported) return false;
	// The map seed sharpens room memory but nothing depends on it: a failed
	// check leaves the room rectangles as the only identity, which is still
	// enough to tell one layout from another.
	NativeDrlg::SetAvailable(LayoutCheck::Verify(context, context->exeBase, DrlgLayout::Checks, "rooms",
		"room memory falls back to the room layout alone"));

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
		using D2RL::Input::Key;
		using D2RL::Input::Modifier;
		struct Entry { const char* id; const char* label; Key key; Modifier modifier; Request request; };
		// Ctrl+Numpad mirrors the right stick for testing without a controller.
		for (const Entry& entry : {
			Entry{"survey-repeat", "Repeat the current map object", Key::Backslash, Modifier::None, Request::Repeat},
			Entry{"survey-category-previous", "Previous map category", Key::Comma, Modifier::None, Request::PreviousCategory},
			Entry{"survey-category-next", "Next map category", Key::Period, Modifier::None, Request::NextCategory},
			Entry{"survey-stick-up", "Scanner: previous category", Key::Numpad8, Modifier::Control, Request::PreviousCategory},
			Entry{"survey-stick-down", "Scanner: next category", Key::Numpad2, Modifier::Control, Request::NextCategory},
			Entry{"survey-stick-left", "Scanner: previous object", Key::Numpad4, Modifier::Control, Request::PreviousObject},
			Entry{"survey-stick-right", "Scanner: next object", Key::Numpad6, Modifier::Control, Request::NextObject}}) {
			const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = entry.id, .displayName = entry.label, .category = "Accessibility",
				.defaultPrimary = {entry.key, entry.modifier}, .callback = OnAction,
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
	ApplyMode();

	PollStick();

	if (const Request request = g_request.exchange(Request::None); request != Request::None && !uiOwnsSpeech) {
		// The backslash, comma and period keys go through the same gate as the stick.
		if (!RadarUi::InPlay()) {
			AsyncLog::Info(context, "[survey] request ignored: not in play");
			return;
		}
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
	ForgetOtherGames(state);
	g_registry.Select(state.level, !g_fullAssistance);
	Radar::World world;
	Scene scene;
	if (!NativeWorld::Read(state.player, state.session, state.level, world) || !Collect(state, scene)) return;
	LogAreas(context, state);
	const auto grid = Navigation::Build(world, SurveyRadius);
	Observe(state, world, grid, scene);
	// Rooms are marked explored by standing in them, so this has to keep up
	// with the player rather than wait for the next catalogue question.
	g_rooms.Observe(state.level, NativeDrlg::MapSeed(NativeWorld::PlayerRoom(), state.level),
		CurrentRooms(state.level, state.session), world.player);
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
