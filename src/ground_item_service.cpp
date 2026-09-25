#include "ground_item_service.h"
#include "ground_items.h"
#include "ground_item_text.h"
#include "entity_layout.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_world.h"
#include "item_audio.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "async_log.h"
#include "zone_transitions.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <string_view>
#include <unordered_set>

namespace D2RAccess::GroundItemService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, SpeechOn, SpeechOff, SpeechToggle, SoundOn, SoundOff, SoundToggle, ExitsOn, ExitsOff, ExitsToggle,
	ChestsOn, ChestsOff, ChestsToggle, ChestsDump, Status };
struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0, revision = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
GroundItems::Tracker g_tracker;
bool g_supported = false, g_exitsSupported = false, g_boundariesSupported = false;
bool g_speech = true, g_sound = true, g_exits = true, g_chests = true, g_suspended = true;
uint64_t g_revision = 0, g_lastPoll = 0, g_entitiesRead = 0;
// Fixed sources share a slow unit read; the tracker updates listener-relative audio.
std::vector<GroundItems::Item> g_landmarks, g_containers, g_questObjects;
std::vector<std::string> g_nearbyLabels;
uint64_t g_labelsUpdated = 0;
void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) g_state.ready = false;
	if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; }
	++g_state.revision;
	ItemAudio::Silence();
}
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	Command next = Command::None;
	if (arg == "on") next = Command::SpeechOn;
	else if (arg == "off") next = Command::SpeechOff;
	else if (arg == "sound-on") next = Command::SoundOn;
	else if (arg == "sound-off") next = Command::SoundOff;
	else if (arg == "exits-on") next = Command::ExitsOn;
	else if (arg == "exits-off") next = Command::ExitsOff;
	else if (arg == "chests-on") next = Command::ChestsOn;
	else if (arg == "chests-off") next = Command::ChestsOff;
	else if (arg == "chests-dump") next = Command::ChestsDump;
	else if (arg == "status") next = Command::Status;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	g_command = next;
	return D2RL::ConsoleCommandResult::Handled;
}
auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* data) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed)
		return D2RL::Input::ActionResult::Ignored;
	g_command = static_cast<Command>(reinterpret_cast<uintptr_t>(data));
	return D2RL::Input::ActionResult::Handled;
}
// Level exits, portals and waypoints as loop-only landmarks for the item mixer.
std::vector<GroundItems::Item> Landmarks(const std::vector<EntityRadar::Entity>& entities) {
	std::vector<GroundItems::Item> result;
	std::unordered_set<uint64_t> fixedExits;
	std::unordered_set<uint32_t> unitExits;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Exit) continue;
		if (entity.placeId) {
			if (!fixedExits.insert(entity.placeId).second) continue;
		} else if (!unitExits.insert(entity.id).second) continue;
		GroundItems::Item landmark;
		landmark.id = entity.id; landmark.position = {entity.position.x, entity.position.y};
		// Loaded exit units require interaction, including dungeon stairs.
		// Walkable crossings come separately from ZoneTransitions::Find.
		landmark.sound = GroundItems::Sound::Portal;
		landmark.source = GroundItems::SourceKind::Object;
		result.push_back(std::move(landmark));
	}
	return result;
}
void AddTransitions(std::vector<GroundItems::Item>& landmarks, const std::vector<EntityRadar::Entity>& entities,
	const std::vector<Radar::LevelExit>& transitions) {
	std::unordered_set<uint32_t> used;
	for (const auto& landmark : landmarks) used.insert(landmark.id);
	for (const auto& transition : transitions) {
		const bool knownWarp = std::any_of(entities.begin(), entities.end(), [&](const auto& entity) {
			return entity.kind == EntityRadar::Kind::Exit && entity.category == EntityRadar::Category::Exit
				&& entity.destinationLevel == transition.level
				&& entity.position.x == transition.position.x && entity.position.y == transition.position.y;
		});
		if (knownWarp) continue;
		uint32_t id = std::numeric_limits<uint32_t>::max() - transition.level;
		while (!used.insert(id).second) --id;
		GroundItems::Item landmark;
		landmark.id = id;
		landmark.position = {transition.position.x, transition.position.y};
		landmark.sound = GroundItems::Sound::ZoneTransition;
		landmark.source = GroundItems::SourceKind::Object;
		landmarks.push_back(std::move(landmark));
	}
}
// Native selection state removes used quest objects and opened containers.
std::vector<GroundItems::Item> ObjectSources(const std::vector<EntityRadar::Entity>& entities, bool quests) {
	std::vector<GroundItems::Item> result;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Object) continue;
		if (quests ? !entity.questObject : (!entity.container || entity.questObject)) continue;
		GroundItems::Item source;
		source.id = entity.id; source.position = {entity.position.x, entity.position.y};
		source.sound = quests ? GroundItems::Sound::Quest : GroundItems::Sound::Chest;
		source.source = GroundItems::SourceKind::Object;
		result.push_back(std::move(source));
	}
	return result;
}
void Suspend(const D2RL::PluginContext* context, const char* reason) {
	ItemAudio::Silence();
	g_nearbyLabels.clear();
	g_landmarks.clear(); g_containers.clear(); g_questObjects.clear(); g_entitiesRead = 0;
	if (!g_suspended) AsyncLog::InfoF(context, "[items] suspended: %s", reason);
	g_suspended = true;
}
}
bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_supported = NativeGroundItems::Initialize(context);
	g_exitsSupported = g_supported && NativeEntities::Initialize(context);
	g_boundariesSupported = g_exitsSupported && NativeWorld::Initialize(context);
	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged, Kind::PlayerResurrected}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		struct Shortcut { const char* id; const char* label; D2RL::Input::Key key; Command command; };
		for (const auto& shortcut : {Shortcut{"item-speech", "Toggle nearby item speech", D2RL::Input::Key::I, Command::SpeechToggle},
			Shortcut{"item-sounds", "Toggle spatial item loops", D2RL::Input::Key::L, Command::SoundToggle},
			Shortcut{"exit-sounds", "Toggle level exit loops", D2RL::Input::Key::P, Command::ExitsToggle},
			Shortcut{"chest-sounds", "Toggle container loops", D2RL::Input::Key::K, Command::ChestsToggle}}) {
			const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize, .logicalId = shortcut.id,
				.displayName = shortcut.label, .category = "Accessibility", .defaultPrimary = {shortcut.key, D2RL::Input::Modifier::Control},
				.callback = Action, .userData = reinterpret_cast<void*>(shortcut.command)};
			D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
			if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
		}
	}
	g_chests = RadarConfig::ReadChests(context).enabled.value_or(g_chests);
	AsyncLog::InfoF(context, "[items] containers enabled=%u", g_chests);
	(void)context->RegisterConsoleCommand("access-items", Console,
		"Ground items: on|off|sound-on|sound-off|exits-on|exits-off|chests-on|chests-off|chests-dump|status."
		" Ctrl+I speech, Ctrl+L item loops, Ctrl+P exit loops, Ctrl+K container loops.");
	(void)ItemAudio::Start(context);
	return true;
}
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	const auto command = g_command.exchange(Command::None);
	if (command == Command::SpeechOn || command == Command::SpeechOff || command == Command::SpeechToggle) {
		g_speech = command == Command::SpeechOn || (command == Command::SpeechToggle && !g_speech);
		ScreenReader::Speak(GroundItemText::Enabled(g_speech), true);
	}
	if (command == Command::SoundOn || command == Command::SoundOff || command == Command::SoundToggle) {
		g_sound = command == Command::SoundOn || (command == Command::SoundToggle && !g_sound);
		if (!g_sound) ItemAudio::Silence();
		g_questObjects.clear(); g_entitiesRead = 0;
		ScreenReader::Speak(GroundItemText::SoundEnabled(g_sound), true);
	}
	if (command == Command::ExitsOn || command == Command::ExitsOff || command == Command::ExitsToggle) {
		g_exits = command == Command::ExitsOn || (command == Command::ExitsToggle && !g_exits);
		if (!g_exits) g_landmarks.clear();
		g_entitiesRead = 0; // refresh on the next poll
		ScreenReader::Speak(GroundItemText::ExitSoundEnabled(g_exits), true);
	}
	if (command == Command::ChestsOn || command == Command::ChestsOff || command == Command::ChestsToggle) {
		g_chests = command == Command::ChestsOn || (command == Command::ChestsToggle && !g_chests);
		if (!g_chests) g_containers.clear();
		g_entitiesRead = 0;
		ScreenReader::Speak(GroundItemText::ChestSoundEnabled(g_chests), true);
	}
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	if (state.revision != g_revision) {
		g_revision = state.revision;
		g_tracker.Reset(); g_nearbyLabels.clear(); g_landmarks.clear(); g_containers.clear(); g_questObjects.clear(); g_entitiesRead = 0;
	}
	if (command == Command::Status) {
		ScreenReader::Speak(std::string(GroundItemText::Enabled(g_speech)) + ". " + GroundItemText::SoundEnabled(g_sound)
			+ ". " + GroundItemText::ExitSoundEnabled(g_exits) + ". " + GroundItemText::ChestSoundEnabled(g_chests), true);
		AsyncLog::InfoF(context, "[items] status supported=%u exits_supported=%u ready=%u audio=%u speech=%u sound=%u exits=%u chests=%u landmarks=%zu containers=%zu quest_objects=%zu",
			g_supported, g_exitsSupported, state.ready, ItemAudio::Available(), g_speech, g_sound, g_exits, g_chests,
			g_landmarks.size(), g_containers.size(), g_questObjects.size());
	}
	// One-shot verdict list: what the container predicate says about every object
	// the player can currently see, so the offline derivation can be checked
	// against a room whose contents are known.
	if (command == Command::ChestsDump) {
		std::vector<EntityRadar::Entity> entities;
		if (!g_exitsSupported || !state.ready || !NativeEntities::Read(state.player, state.session, entities))
			AsyncLog::Info(context, "[items] chests-dump unavailable");
		else {
			size_t objects = 0, containers = 0;
			for (const auto& entity : entities) {
				if (entity.kind != EntityRadar::Kind::Object) continue;
				++objects;
				containers += entity.container;
				AsyncLog::InfoF(context, "[items] object id=%u at=%.0f,%.0f container=%u quest=%u name=%s",
					entity.id, entity.position.x, entity.position.y, entity.container, entity.questObject, entity.name.c_str());
			}
			AsyncLog::InfoF(context, "[items] chests-dump: %zu objects, %zu containers", objects, containers);
			// The tripwire the offline derivation left: a patch that renumbers
			// OperateFn changes this count before it changes any verdict.
			size_t rows = 0, total = 0;
			if (!NativeEntities::CountContainerRows(rows, total))
				AsyncLog::Info(context, "[items] chests-dump: objects.txt unreadable, container rows unchecked");
			else AsyncLog::InfoF(context, "[items] chests-dump: %zu of %zu objects.txt rows are containers, expected %zu of %zu%s",
				rows, total, EntityLayout::ContainerRowCount, EntityLayout::ObjectRowCount,
				rows == EntityLayout::ContainerRowCount && total == EntityLayout::ObjectRowCount
					? "" : " -- rerun tools/objects_containers.py");
		}
	}
	const uint64_t now = GetTickCount64();
	if (command == Command::None && now - g_lastPoll < GroundItems::PollMs) return;
	g_lastPoll = now;
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	if (!g_supported || !state.ready || (!g_speech && !g_sound && !g_exits && !g_chests)) { Suspend(context, "lifecycle/disabled"); return; }
	if (RadarUi::Inspect(process == GetCurrentProcessId()) != RadarUi::State::Playing) { Suspend(context, "UI/focus"); return; }
	GroundItems::Snapshot snapshot;
	if (!NativeGroundItems::Read(state.player, state.session, snapshot)) { Suspend(context, "world unavailable"); return; }
	if (g_suspended) AsyncLog::Info(context, "[items] resumed");
	g_suspended = false;
	g_nearbyLabels.clear();
	g_labelsUpdated = now;
	// Fixed exits, containers and quest objects share one slower unit read.
	if ((g_exits || g_chests || g_sound) && g_exitsSupported && (!g_entitiesRead || now - g_entitiesRead >= GroundItems::ExitRefreshMs)) {
		g_entitiesRead = now;
		std::vector<EntityRadar::Entity> entities;
		const bool read = NativeEntities::Read(state.player, state.session, entities);
		auto landmarks = read && g_exits ? Landmarks(entities) : std::vector<GroundItems::Item>{};
		if (g_exits && g_boundariesSupported) {
			Radar::World world;
			NativeLevelExits::Layout layout;
			if (NativeWorld::Read(state.player, state.session, 0, world)
				&& NativeEntities::ReadLayout(state.player, true, layout))
				AddTransitions(landmarks, entities, ZoneTransitions::Find(world, layout));
		}
		if (landmarks.size() != g_landmarks.size()) AsyncLog::InfoF(context, "[items] exits nearby: %zu", landmarks.size());
		g_landmarks = std::move(landmarks);
		auto containers = read && g_chests ? ObjectSources(entities, false) : std::vector<GroundItems::Item>{};
		if (containers.size() != g_containers.size()) AsyncLog::InfoF(context, "[items] containers nearby: %zu", containers.size());
		g_containers = std::move(containers);
		auto quests = read && g_sound ? ObjectSources(entities, true) : std::vector<GroundItems::Item>{};
		if (quests.size() != g_questObjects.size()) AsyncLog::InfoF(context, "[items] quest objects nearby: %zu", quests.size());
		g_questObjects = std::move(quests);
	}
	snapshot.landmarks = g_landmarks;
	snapshot.containers = g_containers;
	snapshot.questObjects = g_questObjects;
	if (g_speech && !uiOwnsSpeech) for (const auto& item : snapshot.items)
		if (!item.name.empty() && std::hypot(item.position.x - snapshot.player.x, item.position.y - snapshot.player.y) <= GroundItems::SpeakRadius)
			g_nearbyLabels.push_back(item.baseName);
	auto update = g_tracker.Tick(snapshot, now, g_speech && !uiOwnsSpeech && command == Command::None, g_sound, g_chests);
	if (g_sound || g_exits || g_chests) ItemAudio::Update(std::move(update.voices));
	if (!update.speech.empty()) {
		AsyncLog::InfoF(context, "[items] nearby: %s", update.speech.c_str());
		ScreenReader::Speak(update.speech, false);
	}
}
void Unregister() {
	if (g_lifecycle && g_context)
		for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	if (g_input && g_context)
		for (auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_listeners.clear(); g_actions.clear();
	ItemAudio::Stop();
	g_tracker.Reset();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
	g_supported = g_exitsSupported = g_boundariesSupported = false;
	g_speech = g_sound = g_exits = g_chests = true; g_suspended = true;
	g_revision = g_lastPoll = g_entitiesRead = 0; g_command = Command::None; g_landmarks.clear(); g_containers.clear(); g_questObjects.clear();
	g_nearbyLabels.clear(); g_labelsUpdated = 0;
}
bool OwnsWorldLabel(std::string_view label) {
	return g_speech && !g_suspended && GetTickCount64() - g_labelsUpdated <= GroundItems::SnapshotExpiryMs
		&& GroundItems::MatchesLabel(label, g_nearbyLabels);
}
}
