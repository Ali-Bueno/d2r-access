#include "ground_item_service.h"
#include "ground_items.h"
#include "ground_item_text.h"
#include "entity_layout.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_world.h"
#include "entity_cue_audio.h"
#include "entity_radar.h"
#include "item_audio.h"
#include "keyboard_service.h"
#include "doorways.h"
#include "panel_state.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "secret_doors.h"
#include "screen_reader.h"
#include "speech_priority.h"
#include "speech_strings.h"
#include "step_counter.h"
#include "tutorial.h"
#include "async_log.h"
#include "zone_transitions.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_set>

namespace D2RAccess::GroundItemService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, SpeechOn, SpeechOff, SpeechToggle, SoundOn, SoundOff, SoundToggle, ExitsOn, ExitsOff, ExitsToggle,
	ChestsOn, ChestsOff, ChestsToggle, ChestsDump, DoorsOn, DoorsOff, QuestsOn, QuestsOff, StashOn, StashOff, Status };
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
bool g_speech = true, g_sound = true, g_exits = true, g_chests = true, g_doors = true, g_quests = true, g_stash = true, g_suspended = true, g_stashOpen = false;
uint64_t g_revision = 0, g_lastPoll = 0, g_entitiesRead = 0;
// Fixed sources share a slow unit read; the tracker updates listener-relative audio.
std::vector<GroundItems::Item> g_landmarks, g_containers, g_questObjects, g_shutDoors, g_shrines, g_doorways, g_pads;
std::atomic<bool> g_shrineSounds{true}, g_doorwaySounds{true}, g_openDoorSounds{false}, g_stepClicks{false}, g_stepBeep{false}, g_padSounds{true};
// Into StepCounter::Intervals: 5 units (a tester, 2026-10-21: 1 is spam, 10 too coarse indoors).
std::atomic<int> g_stepInterval{1};
std::atomic<bool> g_distanceSteps{false};
StepCounter::Odometer g_odometer;
// The Jail's secret doors, felt as a draft when walked past (docs/secret-doors.md).
std::atomic<bool> g_secretDrafts{true};
SecretDoors::Draft g_draft;
std::vector<std::string> g_nearbyLabels;
uint64_t g_labelsUpdated = 0;
// Every loaded ground item's name, for the pickup click, and the target last clicked for.
std::vector<std::string> g_itemLabels;
std::string g_pickupLabel;
GroundItems::Purse g_purse;
bool g_pickupClick = false; // out of the list until the user's clicks per item type (decisions, 2026-10-13)
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
	else if (arg == "doors-on") next = Command::DoorsOn;
	else if (arg == "doors-off") next = Command::DoorsOff;
	else if (arg == "quests-on") next = Command::QuestsOn;
	else if (arg == "quests-off") next = Command::QuestsOff;
	else if (arg == "stash-on") next = Command::StashOn;
	else if (arg == "stash-off") next = Command::StashOff;
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
// The game's stash layouts, classic and expansion. While one is open the stash loop gives way.
constexpr std::string_view StashPanels[] = {"BankOriginalLayout", "BankExpansionLayout"};
bool StashOpen() {
	const auto& open = PanelState::OpenPanels();
	return std::any_of(open.begin(), open.end(), [](const std::string& name) {
		return std::find(std::begin(StashPanels), std::end(StashPanels), name) != std::end(StashPanels);
	});
}
// Level exits, portals, waypoints and the stash as loop-only landmarks for the item mixer.
// The stash has its own switch; exits share one.
std::vector<GroundItems::Item> Landmarks(const std::vector<EntityRadar::Entity>& entities, bool exits, bool stashes) {
	std::vector<GroundItems::Item> result;
	std::unordered_set<uint64_t> fixedExits;
	std::unordered_set<uint32_t> unitExits;
	for (const auto& entity : entities) {
		const bool stash = entity.category == EntityRadar::Category::Stash;
		if (stash ? !stashes : (entity.kind != EntityRadar::Kind::Exit || !exits)) continue;
		if (entity.placeId) {
			if (!fixedExits.insert(entity.placeId).second) continue;
		} else if (!unitExits.insert(entity.id).second) continue;
		GroundItems::Item landmark;
		landmark.id = entity.id; landmark.position = {entity.position.x, entity.position.y};
		// Loaded exit units require interaction, including dungeon stairs.
		// Walkable crossings come separately from ZoneTransitions::Find.
		landmark.sound = stash ? GroundItems::Sound::Stash
			: entity.category == EntityRadar::Category::Quest ? GroundItems::Sound::Quest : GroundItems::Sound::Portal;
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
// Native selection state removes used quest objects and opened containers; a read tome
// leaves its unread mode. Quest tomes join the quest share with the scroll cue.
std::vector<GroundItems::Item> ObjectSources(const std::vector<EntityRadar::Entity>& entities, bool quests) {
	std::vector<GroundItems::Item> result;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Object) continue;
		if (quests ? !EntityRadar::QuestSource(entity) : (!entity.container || entity.questObject)) continue;
		GroundItems::Item source;
		source.id = entity.id; source.position = {entity.position.x, entity.position.y};
		using GroundItems::Sound;
		source.sound = quests ? (entity.questObject ? Sound::Quest : Sound::Scroll)
			: entity.corpse ? Sound::Corpse : entity.weaponRack ? Sound::WeaponRack : Sound::Chest;
		source.source = GroundItems::SourceKind::Object;
		result.push_back(std::move(source));
	}
	return result;
}
// Shrines and wells the player can still use: a used one is no longer selectable
// and leaves the entity read (a tester: noticed only through the scanner).
std::vector<GroundItems::Item> ShrineSources(const std::vector<EntityRadar::Entity>& entities) {
	std::vector<GroundItems::Item> result;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Object || entity.category != EntityRadar::Category::Shrine) continue;
		GroundItems::Item source;
		source.id = entity.id; source.position = {entity.position.x, entity.position.y};
		source.sound = entity.well ? GroundItems::Sound::Well : GroundItems::Sound::Shrine;
		source.source = GroundItems::SourceKind::Object;
		result.push_back(std::move(source));
	}
	return result;
}
// Teleport pads by where each leads (KeyboardService::PadLeads): bright where the
// player has not stood (docs/keyboard-play.md#where-a-pad-leads-said).
std::vector<GroundItems::Item> PadSources(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, size_t& been) {
	std::vector<GroundItems::Item> result;
	been = 0;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Object || !entity.padClass) continue;
		GroundItems::Item source;
		source.id = entity.id; source.position = {entity.position.x, entity.position.y};
		const bool back = KeyboardService::PadLeads(world, entity) == LevelRoute::Destination::Been;
		source.sound = back ? GroundItems::Sound::PadBeen : GroundItems::Sound::PadNew;
		source.source = GroundItems::SourceKind::Object;
		been += back;
		result.push_back(std::move(source));
	}
	return result;
}
// The user's door recordings for the game's five opening sounds: its "gate" sound belongs
// to the great wooden double doors (docs/ground-items.md#door-loops).
std::optional<GroundItems::Sound> DoorLoop(Generated::DoorSound sound) {
	switch (sound) {
	case Generated::DoorSound::Wood: case Generated::DoorSound::Gate: return GroundItems::Sound::WoodenDoor;
	case Generated::DoorSound::Metal: return GroundItems::Sound::MetalDoor;
	case Generated::DoorSound::Stone: return GroundItems::Sound::StoneDoor;
	case Generated::DoorSound::Slime: return GroundItems::Sound::SlimeDoor;
	case Generated::DoorSound::None: break;
	}
	return std::nullopt;
}
// Shut doors loop; one the player opens falls silent unless `opened` keeps it.
std::vector<GroundItems::Item> DoorSources(const std::vector<EntityRadar::Entity>& entities, bool opened) {
	std::vector<GroundItems::Item> result;
	for (const auto& entity : entities) {
		const auto door = opened && entity.shutDoor == Generated::DoorSound::None ? entity.openedDoor : entity.shutDoor;
		const auto sound = entity.kind == EntityRadar::Kind::Object ? DoorLoop(door) : std::nullopt;
		if (!sound) continue;
		GroundItems::Item source;
		source.id = entity.id; source.position = {entity.position.x, entity.position.y};
		source.sound = *sound;
		source.source = GroundItems::SourceKind::Object;
		result.push_back(std::move(source));
	}
	return result;
}
// The doorways in the walls, at their middle (Doorways::Find).
std::vector<GroundItems::Item> DoorwaySources(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities) {
	std::vector<GroundItems::Item> result;
	for (const auto& doorway : Doorways::Find(world, entities)) {
		GroundItems::Item source;
		source.id = (static_cast<uint32_t>(doorway.centre.x) << 16) | (static_cast<uint32_t>(doorway.centre.y) & 0xFFFF);
		source.position = {doorway.centre.x, doorway.centre.y};
		source.sound = GroundItems::Sound::Doorway;
		source.source = GroundItems::SourceKind::Object;
		result.push_back(std::move(source));
	}
	return result;
}
void Suspend(const D2RL::PluginContext* context, const char* reason) {
	ItemAudio::Silence();
	g_nearbyLabels.clear();
	g_odometer.Reset();
	g_landmarks.clear(); g_containers.clear(); g_questObjects.clear(); g_shutDoors.clear(); g_shrines.clear(); g_doorways.clear(); g_pads.clear(); g_entitiesRead = 0;
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
	g_doors = RadarConfig::ReadDoors(context).enabled.value_or(g_doors);
	AsyncLog::InfoF(context, "[items] containers enabled=%u doors enabled=%u", g_chests, g_doors);
	(void)context->RegisterConsoleCommand("access-items", Console,
		"Ground items: on|off|sound-on|sound-off|exits-on|exits-off|chests-on|chests-off|chests-dump|doors-on|doors-off|quests-on|quests-off|stash-on|stash-off|status."
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
		g_entitiesRead = 0;
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
	if (command == Command::DoorsOn || command == Command::DoorsOff) {
		g_doors = command == Command::DoorsOn;
		if (!g_doors) g_shutDoors.clear(); g_shrines.clear();
		g_entitiesRead = 0;
		ScreenReader::Speak(GroundItemText::DoorSoundEnabled(g_doors), true);
	}
	if (command == Command::QuestsOn || command == Command::QuestsOff) {
		g_quests = command == Command::QuestsOn;
		if (!g_quests) g_questObjects.clear();
		g_entitiesRead = 0;
		ScreenReader::Speak(GroundItemText::QuestSoundEnabled(g_quests), true);
	}
	if (command == Command::StashOn || command == Command::StashOff) {
		g_stash = command == Command::StashOn;
		g_entitiesRead = 0;
		ScreenReader::Speak(GroundItemText::StashSoundEnabled(g_stash), true);
	}
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	if (state.revision != g_revision) {
		g_revision = state.revision;
		g_purse.Reset();
		g_tracker.Reset(); g_nearbyLabels.clear(); g_landmarks.clear(); g_containers.clear(); g_questObjects.clear(); g_shutDoors.clear(); g_shrines.clear();
		g_entitiesRead = 0;
	}
	if (command == Command::Status) {
		ScreenReader::Speak(std::string(GroundItemText::Enabled(g_speech)) + ". " + GroundItemText::SoundEnabled(g_sound)
			+ ". " + GroundItemText::ExitSoundEnabled(g_exits) + ". " + GroundItemText::ChestSoundEnabled(g_chests)
			+ ". " + GroundItemText::DoorSoundEnabled(g_doors) + ". " + GroundItemText::QuestSoundEnabled(g_quests)
			+ ". " + GroundItemText::StashSoundEnabled(g_stash), true);
		AsyncLog::InfoF(context, "[items] status supported=%u exits_supported=%u ready=%u audio=%u speech=%u sound=%u exits=%u chests=%u doors=%u quests=%u stash=%u landmarks=%zu containers=%zu quest_objects=%zu shut_doors=%zu",
			g_supported, g_exitsSupported, state.ready, ItemAudio::Available(), g_speech, g_sound, g_exits, g_chests, g_doors, g_quests, g_stash,
			g_landmarks.size(), g_containers.size(), g_questObjects.size(), g_shutDoors.size());
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
				AsyncLog::InfoF(context, "[items] object id=%u at=%.0f,%.0f container=%u corpse=%u quest=%u shut_door=%u name=%s",
					entity.id, entity.position.x, entity.position.y, entity.container, entity.corpse, entity.questObject,
					static_cast<unsigned>(entity.shutDoor), entity.name.c_str());
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
	const bool playing = RadarUi::Inspect(process == GetCurrentProcessId()) == RadarUi::State::Playing;
	if (const int gain = g_purse.Update(g_supported && state.ready ? NativeGroundItems::PlayerGold() : -1, playing); gain > 0 && g_speech) {
		char phrase[100]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().goldPickedUp, gain);
		AsyncLog::InfoF(context, "[items] gold picked up: %d", gain);
		ScreenReader::Speak(phrase, false);
	}
	const bool doorways = g_doorwaySounds.load(), steps = g_stepClicks.load(), pads = g_padSounds.load();
	if (!g_supported || !state.ready || (!g_speech && !g_sound && !g_exits && !g_chests && !g_doors && !g_quests && !g_stash && !doorways && !steps && !pads)) {
		Suspend(context, "lifecycle/disabled");
		return;
	}
	if (!playing) { Suspend(context, "UI/focus"); return; }
	GroundItems::Snapshot snapshot;
	if (!NativeGroundItems::Read(state.player, state.session, snapshot)) { Suspend(context, "world unavailable"); return; }
	if (g_suspended) AsyncLog::Info(context, "[items] resumed");
	g_suspended = false;
	if (!steps) g_odometer.Reset();
	else if (g_odometer.Walk(snapshot.player, snapshot.session, StepCounter::Intervals[g_stepInterval.load()]) > 0)
		ItemAudio::PlayStep(g_stepBeep.load() ? GroundItems::Sound::Step : GroundItems::Sound::StepClick);
	g_nearbyLabels.clear();
	g_labelsUpdated = now;
	// Fixed exits, containers, quest objects and doors share one slower unit read.
	const bool shrines = g_shrineSounds.load(), drafts = g_secretDrafts.load();
	if ((g_exits || g_chests || g_quests || g_doors || g_stash || shrines || doorways || drafts || pads) && g_exitsSupported && (!g_entitiesRead || now - g_entitiesRead >= GroundItems::ExitRefreshMs)) {
		g_entitiesRead = now;
		std::vector<EntityRadar::Entity> entities;
		const bool read = NativeEntities::Read(state.player, state.session, entities);
		auto landmarks = read && (g_exits || g_stash) ? Landmarks(entities, g_exits, g_stash) : std::vector<GroundItems::Item>{};
		const bool borders = g_exits && g_boundariesSupported;
		Radar::World world;
		const bool worldRead = (borders || doorways || pads) && NativeWorld::Read(state.player, state.session, 0, world);
		if (borders && worldRead) {
			NativeLevelExits::Layout layout;
			if (NativeEntities::ReadLayout(state.player, true, layout)) AddTransitions(landmarks, entities, ZoneTransitions::Find(world, layout));
		}
		if (landmarks.size() != g_landmarks.size()) AsyncLog::InfoF(context, "[items] exits nearby: %zu", landmarks.size());
		g_landmarks = std::move(landmarks);
		auto containers = read && g_chests ? ObjectSources(entities, false) : std::vector<GroundItems::Item>{};
		if (containers.size() != g_containers.size()) AsyncLog::InfoF(context, "[items] containers nearby: %zu", containers.size());
		g_containers = std::move(containers);
		auto quests = read && g_quests ? ObjectSources(entities, true) : std::vector<GroundItems::Item>{};
		if (quests.size() != g_questObjects.size()) AsyncLog::InfoF(context, "[items] quest objects nearby: %zu", quests.size());
		g_questObjects = std::move(quests);
		auto doors = read && g_doors ? DoorSources(entities, g_openDoorSounds.load()) : std::vector<GroundItems::Item>{};
		if (doors.size() != g_shutDoors.size()) AsyncLog::InfoF(context, "[items] shut doors nearby: %zu", doors.size());
		g_shutDoors = std::move(doors);
		auto gaps = worldRead && doorways ? DoorwaySources(world, entities) : std::vector<GroundItems::Item>{};
		if (gaps.size() != g_doorways.size()) AsyncLog::InfoF(context, "[items] doorways nearby: %zu level=%d", gaps.size(), world.level);
		g_doorways = std::move(gaps);
		size_t been = 0;
		auto padLoops = worldRead && pads ? PadSources(world, entities, been) : std::vector<GroundItems::Item>{};
		if (padLoops.size() != g_pads.size()) AsyncLog::InfoF(context, "[items] teleport pads nearby: %zu, %zu leading where the player has stood", padLoops.size(), been);
		g_pads = std::move(padLoops);
		auto wells = read && shrines ? ShrineSources(entities) : std::vector<GroundItems::Item>{};
		if (wells.size() != g_shrines.size()) AsyncLog::InfoF(context, "[items] shrines and wells nearby: %zu", wells.size());
		g_shrines = std::move(wells);
		if (read && drafts) {
			std::vector<Radar::Position> secrets;
			for (const auto& entity : entities) if (entity.secretDoor) secrets.push_back(entity.position);
			if (g_draft.Felt(secrets, {snapshot.player.x, snapshot.player.y}) && !uiOwnsSpeech) {
				AsyncLog::InfoF(context, "[items] secret door felt: %zu shut loaded, one at %.0f,%.0f", secrets.size(),
					secrets.front().x, secrets.front().y);
				ScreenReader::Speak(Strings::Current().secretDraft, false);
			}
		}
	}
	snapshot.landmarks = g_landmarks;
	if (const bool stashOpen = StashOpen(); stashOpen != g_stashOpen) {
		g_stashOpen = stashOpen;
		AsyncLog::InfoF(context, "[items] stash %s: its loop %s", stashOpen ? "open" : "closed", stashOpen ? "paused" : "resumed");
	}
	if (g_stashOpen) std::erase_if(snapshot.landmarks, [](const GroundItems::Item& landmark) { return landmark.sound == GroundItems::Sound::Stash; });
	snapshot.containers = g_containers;
	snapshot.questObjects = g_questObjects;
	snapshot.doors = g_shutDoors;
	if (shrines) snapshot.shrines = g_shrines;
	if (doorways) snapshot.doorways = g_doorways;
	if (pads) snapshot.pads = g_pads;
	g_itemLabels.clear();
	for (const auto& item : snapshot.items)
		if (!item.baseName.empty()) g_itemLabels.push_back(item.baseName);
	if (g_speech && !uiOwnsSpeech) for (const auto& item : snapshot.items)
		if (!item.name.empty() && std::hypot(item.position.x - snapshot.player.x, item.position.y - snapshot.player.y) <= GroundItems::SpeakRadius)
			g_nearbyLabels.push_back(item.baseName);
	auto update = g_tracker.Tick(snapshot, now, g_speech && !uiOwnsSpeech && command == Command::None, g_sound, g_chests, g_doors, g_quests, shrines);
	if (Tutorial::QuietWorld()) ItemAudio::Silence();
	else if (g_sound || g_exits || g_chests || g_doors || g_quests || g_stash || shrines || doorways) ItemAudio::Update(std::move(update.voices));
	if (!update.speech.empty()) {
		AsyncLog::InfoF(context, "[items] nearby: %s", update.speech.c_str());
		SpeechPriority::Say(SpeechPriority::Kind::NearbyItems, update.speech);
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
	g_draft.Reset();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
	g_supported = g_exitsSupported = g_boundariesSupported = false;
	g_speech = g_sound = g_exits = g_chests = g_quests = g_stash = true; g_suspended = true; g_stashOpen = false;
	g_revision = g_lastPoll = g_entitiesRead = 0; g_command = Command::None; g_landmarks.clear(); g_containers.clear(); g_questObjects.clear(); g_shutDoors.clear(); g_shrines.clear();
	g_nearbyLabels.clear(); g_labelsUpdated = 0; g_itemLabels.clear(); g_pickupLabel.clear();
}
void NoteWorldTarget(std::string_view label) {
	const bool item = g_pickupClick && !g_suspended && !label.empty()
		&& GetTickCount64() - g_labelsUpdated <= GroundItems::SnapshotExpiryMs && GroundItems::MatchesLabel(label, g_itemLabels);
	if (item && label != g_pickupLabel) EntityCueAudio::Play(EntityCueAudio::Sound::Pickup, {});
	g_pickupLabel = item ? std::string(label) : std::string{};
}
bool ShrineSounds() { return g_shrineSounds.load(); }
void SetShrineSounds(bool enabled) { g_shrineSounds = enabled; }
bool SecretDrafts() { return g_secretDrafts.load(); }
void SetSecretDrafts(bool enabled) { g_secretDrafts = enabled; }
bool DoorwaySounds() { return g_doorwaySounds.load(); }
void SetDoorwaySounds(bool enabled) { g_doorwaySounds = enabled; }
bool PadSounds() { return g_padSounds.load(); }
void SetPadSounds(bool enabled) { g_padSounds = enabled; }
bool OpenDoorSounds() { return g_openDoorSounds.load(); }
void SetOpenDoorSounds(bool enabled) { g_openDoorSounds = enabled; }
bool StepClicks() { return g_stepClicks.load(); }
void SetStepClicks(bool enabled) { g_stepClicks = enabled; }
bool StepBeep() { return g_stepBeep.load(); }
void SetStepBeep(bool beep) { g_stepBeep = beep; }
int StepInterval() { return g_stepInterval.load(); }
void SetStepInterval(int interval) {
	g_stepInterval = std::clamp(interval, 0, static_cast<int>(std::size(StepCounter::Intervals)) - 1);
	EntityRadar::SetUnitsPerStep(g_distanceSteps ? StepCounter::Intervals[static_cast<size_t>(g_stepInterval.load())] : 0);
}
bool DistanceSteps() { return g_distanceSteps.load(); }
void SetDistanceSteps(bool enabled) {
	g_distanceSteps = enabled;
	SetStepInterval(g_stepInterval.load());
}
bool PickupClick() { return g_pickupClick; }
void SetPickupClick(bool enabled) { g_pickupClick = enabled; }

bool OwnsWorldLabel(std::string_view label) {
	return g_speech && !g_suspended && GetTickCount64() - g_labelsUpdated <= GroundItems::SnapshotExpiryMs
		&& GroundItems::MatchesLabel(label, g_nearbyLabels);
}

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled(Switch which) {
	return which == Switch::Speech ? g_speech : which == Switch::Sounds ? g_sound
		: which == Switch::Exits ? g_exits : which == Switch::Chests ? g_chests
		: which == Switch::Quests ? g_quests : which == Switch::Stash ? g_stash : g_doors;
}
void SetEnabled(Switch which, bool enabled) {
	g_command = which == Switch::Speech ? (enabled ? Command::SpeechOn : Command::SpeechOff)
		: which == Switch::Sounds ? (enabled ? Command::SoundOn : Command::SoundOff)
		: which == Switch::Exits ? (enabled ? Command::ExitsOn : Command::ExitsOff)
		: which == Switch::Chests ? (enabled ? Command::ChestsOn : Command::ChestsOff)
		: which == Switch::Quests ? (enabled ? Command::QuestsOn : Command::QuestsOff)
		: which == Switch::Stash ? (enabled ? Command::StashOn : Command::StashOff)
		: (enabled ? Command::DoorsOn : Command::DoorsOff);
}
}
