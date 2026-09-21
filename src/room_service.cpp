#include "room_service.h"
#include "async_log.h"
#include "beacon_service.h"
#include "entity_radar.h"
#include "exit_review.h"
#include "generated/level_flags.h"
#include "input_direction.h"
#include "native_entities.h"
#include "native_level_exits.h"
#include "native_world.h"
#include "npc_proximity.h"
#include "radar_ui.h"
#include "rooms.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::RoomService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;

enum class Command { None, On, Off, Toggle, Status };

// Levels.txt DrlgType: 1 is a maze assembled from LvlPrest cells, so one
// streamed room is one chamber. Types 2 and 3 subdivide a hand-built or open
// level, where a room boundary means nothing on the ground.
constexpr int32_t MazeDrlgType = 1;
// Walking into a room is slow; nothing here needs the 50 ms UI poll.
constexpr uint64_t PollMs = 250;

struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
std::atomic<bool> g_enabled{true}, g_asked{false}, g_review{false};
bool g_supported = false;
Rooms::Hysteresis g_rooms;
// Distances are spoken by the same rule as the entity scan and the catalogue.
const EntityRadar::Settings g_scan;
// The exit cycle, and the room and level it belongs to.
ExitReview::Cycle g_cycle;
Rooms::RoomKey g_reviewKey;
int g_reviewLevel = 0;
uint64_t g_nextPoll = 0;

void Forget() {
	g_rooms.Forget();
	g_cycle.Reset();
	g_reviewKey = {};
	g_reviewLevel = 0;
}

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) g_state.level = event->currentValue;
	Forget();
}
const char* Where(Radar::Position player, Radar::Position target) {
	return Strings::Current().scanDirections[static_cast<size_t>(NpcProximity::DirectionOf(player, target))];
}
// The level exits standing inside this room keep their own names; every other
// way out is spoken as a direction alone.
void AppendWarps(const Lifecycle& state, const Rooms::Room& room, Radar::Position player, std::string& line) {
	const auto& strings = Strings::Current();
	std::vector<EntityRadar::Entity> entities;
	if (!NativeEntities::Read(state.player, state.session, entities)) return;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Exit || !room.Contains(entity.position)) continue;
		char text[192]{};
		std::snprintf(text, sizeof text, strings.roomExitNamed, entity.name.c_str(), Where(player, entity.position));
		if (!line.empty()) line += strings.listSeparator;
		line += text;
	}
}
// "Exits: to the left, ahead, Den of Evil to the right" -- every way out of
// the room, the plain openings by direction and the level exits by name.
void Announce(const Lifecycle& state, const Radar::World& world, const Rooms::Room& room, const char* why) {
	const auto& strings = Strings::Current();
	std::string line;
	for (const auto& opening : room.openings) {
		if (!line.empty()) line += strings.listSeparator;
		line += Where(world.player, opening.centre);
	}
	AppendWarps(state, room, world.player, line);
	char text[512]{};
	if (line.empty()) std::snprintf(text, sizeof text, "%s", strings.roomNoExits);
	else std::snprintf(text, sizeof text, strings.roomExits, line.c_str());
	ScreenReader::Speak(text, true);
	AsyncLog::InfoF(g_context, "[room] %s at=%d,%d size=%dx%d openings=%zu level=%d -> %s",
		why, room.x, room.y, room.width, room.height, room.openings.size(), state.level, text);
}
// BACK+Y: the room the player stands in, asked for. Answers whether or not the
// automatic announcements are on -- that toggle is about speaking unasked.
void Layout(const Lifecycle& state) {
	const auto& strings = Strings::Current();
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) {
		ScreenReader::Speak(strings.roomsUnavailable, true);
		AsyncLog::InfoF(g_context, "[room] layout asked: world unavailable level=%d", state.level);
		return;
	}
	Rooms::Room room;
	if (!Maze(state.level) || !Rooms::Describe(world, world.player, room)) {
		ScreenReader::Speak(strings.roomOutside, true);
		AsyncLog::InfoF(g_context, "[room] layout asked: not in a dungeon room level=%d maze=%u",
			state.level, Maze(state.level) ? 1u : 0u);
		return;
	}
	Announce(state, world, room, "asked");
}
// The level exits to review: the ones inside this room on a maze level; every
// known one of the level everywhere else, where a room boundary means
// nothing -- including a maze level where the player stands in no streamed
// room at all, which falls back to the same full list.
std::vector<EntityRadar::Entity> Exits(const Lifecycle& state, bool maze, bool inRoom, const Rooms::Room& room) {
	std::vector<EntityRadar::Entity> entities, exits;
	if (!NativeEntities::Read(state.player, state.session, entities, true)) return exits;
	for (auto& entity : entities)
		if (entity.kind == EntityRadar::Kind::Exit && (!maze || !inRoom || room.Contains(entity.position)))
			exits.push_back(std::move(entity));
	return exits;
}
// A plain opening is not a unit guidance can re-read, so it is offered as a
// fixed place: the room kind, which guidance never re-locates.
EntityRadar::Entity Place(const ExitReview::Item& item) {
	EntityRadar::Entity entity;
	entity.kind = EntityRadar::Kind::Room;
	entity.position = item.point;
	entity.name = item.label;
	entity.levelExitId = NativeLevelExits::Identity(item.point);
	entity.id = static_cast<uint32_t>(entity.levelExitId) | 1u; // never zero: guidance ignores id 0
	return entity;
}
// One way out per request: nearest first, cycling, and offered to guidance so
// R3 or Ctrl+B can lock it straight after.
void Review(const D2RL::PluginContext* context, const Lifecycle& state, uint64_t now) {
	const auto& strings = Strings::Current();
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) {
		ScreenReader::Speak(strings.roomsUnavailable, true);
		return;
	}
	const bool maze = Maze(state.level);
	Rooms::Room room;
	const bool inRoom = maze && Rooms::Describe(world, world.player, room);
	const Rooms::RoomKey key = inRoom ? Rooms::RoomKey{room.x, room.y, true} : Rooms::RoomKey{};
	if (!(key == g_reviewKey) || state.level != g_reviewLevel) {
		g_cycle.Reset();
		g_reviewKey = key;
		g_reviewLevel = state.level;
	}
	const std::vector<Rooms::Opening> openings = inRoom ? room.openings : std::vector<Rooms::Opening>{};
	const auto items = ExitReview::Collect(world.player, openings, Exits(state, maze, inRoom, room), strings);
	const size_t index = g_cycle.Next(items);
	if (index == ExitReview::NoItem) {
		ScreenReader::Speak(strings.roomNoExits, true);
		AsyncLog::InfoF(context, "[room] review empty level=%d maze=%u in_room=%u",
			state.level, maze ? 1u : 0u, inRoom ? 1u : 0u);
		return;
	}
	const auto& item = items[index];
	const auto text = ExitReview::Describe(item, g_scan, strings);
	ScreenReader::Speak(text, true);
	BeaconService::Offer(item.named ? item.entity : Place(item), state.level, now);
	AsyncLog::InfoF(context, "[room] review %zu of %zu at=%.0f,%.0f named=%u -> %s",
		index + 1, items.size(), item.point.x, item.point.y, item.named ? 1u : 0u, text.c_str());
}
auto Next(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize)
		|| event->kind != D2RL::Input::ActionEventKind::Pressed) return D2RL::Input::ActionResult::Ignored;
	g_review = true;
	return D2RL::Input::ActionResult::Handled;
}
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") g_command = Command::On;
	else if (arg == "off") g_command = Command::Off;
	else if (arg == "layout") g_asked = true;
	else if (arg == "next") g_review = true;
	else if (arg.empty() || arg == "status") g_command = Command::Status;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
auto Toggle(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize)
		|| event->kind != D2RL::Input::ActionEventKind::Pressed) return D2RL::Input::ActionResult::Ignored;
	g_command = Command::Toggle;
	return D2RL::Input::ActionResult::Handled;
}
}
bool Maze(int level) {
	return Generated::FindLevelFlags(static_cast<int32_t>(level)).drlgType == MazeDrlgType;
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_supported = NativeWorld::Initialize(context) && NativeEntities::Initialize(context);
	(void)context->RegisterConsoleCommand("access-rooms",
		Console, "Room ways out: on|off|status, layout to hear the room you are in, next to review them one at a time. Ctrl+M, Ctrl+V; BACK+Y and BACK+A on the pad.");
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		const D2RL::Input::ActionRegistration actions[]{
			{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = "rooms-toggle", .displayName = "Toggle room exit announcements", .category = "Accessibility",
				.defaultPrimary = {D2RL::Input::Key::M, D2RL::Input::Modifier::Control}, .callback = Toggle},
			{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = "rooms-next-exit", .displayName = "Review the next way out", .category = "Accessibility",
				.defaultPrimary = {D2RL::Input::Key::V, D2RL::Input::Modifier::Control}, .callback = Next}};
		for (const auto& action : actions) {
			D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
			if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
		}
	}
	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{
			.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle)
			!= D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	AsyncLog::InfoF(context, "[room] supported=%u", g_supported ? 1u : 0u);
	return g_supported;
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	const auto command = g_command.exchange(Command::None);
	// Both sources are taken every tick, in a menu too, so a press made there
	// cannot be answered later.
	const bool asked = InputDirection::TakeBackWith(InputDirection::FaceButton::Y) || g_asked.exchange(false);
	const bool review = InputDirection::TakeBackWith(InputDirection::FaceButton::A) || g_review.exchange(false);
	if (command != Command::None) {
		if (command != Command::Status) {
			g_enabled = command == Command::On || (command == Command::Toggle && !g_enabled);
			std::lock_guard lock(g_mutex);
			Forget();
		}
		const auto& strings = Strings::Current();
		ScreenReader::Speak(!g_supported ? strings.roomsUnavailable
			: g_enabled ? strings.roomsEnabled : strings.roomsDisabled, true);
		AsyncLog::InfoF(context, "[room] enabled=%u supported=%u", g_enabled ? 1u : 0u, g_supported ? 1u : 0u);
		return;
	}
	if (!g_supported) {
		if (asked || review) ScreenReader::Speak(Strings::Current().roomsUnavailable, true);
		return;
	}
	const uint64_t now = GetTickCount64();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	// An asked-for readout answers even while the UI reader is talking; the
	// automatic one waits, exactly as the entity scan and its proximity poll do.
	if (asked && state.ready && RadarUi::InPlay()) {
		std::lock_guard lock(g_mutex);
		Layout(state);
	}
	if (review && state.ready && RadarUi::InPlay()) {
		std::lock_guard lock(g_mutex);
		Review(context, state, now);
	}
	if (!g_enabled || uiOwnsSpeech || now - g_nextPoll < PollMs) return;
	g_nextPoll = now;
	if (!state.ready || !Maze(state.level) || !RadarUi::InPlay()) return;
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return;
	Rooms::Room room;
	const bool inRoom = Rooms::Describe(world, world.player, room);
	std::lock_guard lock(g_mutex);
	if (!g_rooms.Observe(inRoom ? Rooms::RoomKey{room.x, room.y, true} : Rooms::RoomKey{},
		static_cast<long long>(now))) return;
	Announce(state, world, room, "entered");
}

void Unregister() {
	if (g_lifecycle) for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	g_listeners.clear();
	if (g_input) for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_actions.clear();
	std::lock_guard lock(g_mutex);
	Forget();
	g_state = {};
	g_enabled = true; g_asked = false; g_review = false; g_supported = false; g_nextPoll = 0;
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
}
}
