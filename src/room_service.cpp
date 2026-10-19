#include "room_service.h"
#include "async_log.h"
#include "beacon_service.h"
#include "entity_radar.h"
#include "exit_review.h"
#include "generated/level_flags.h"
#include "input_direction.h"
#include "level_exit_layout.h"
#include "native_entities.h"
#include "native_level_exits.h"
#include "native_world.h"
#include "radar_ui.h"
#include "rooms.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "spoken_direction.h"
#include "terrain_service.h"

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
constexpr int32_t MazeDrlgType = 1, OutdoorDrlgType = 3;
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
std::atomic<bool> g_enabled{true}, g_asked{false}, g_finderToggle{false};
// The exit finder: while on, in a dungeon room, a right-stick push names the
// ways out on that side. Off by default, for the session.
std::atomic<bool> g_finder{false};
// Ways out told by the map's sides, the room's own walls, instead of the
// screen's: a tester's request of 2026-10-14, on by default (the user).
std::atomic<bool> g_mapSides{true};
ExitReview::Sides Sides() { return g_mapSides.load(std::memory_order_relaxed) ? ExitReview::Sides::Map : ExitReview::Sides::Screen; }
bool g_supported = false;
Rooms::Hysteresis g_rooms;
// Distances are spoken by the same rule as the entity scan and the catalogue.
const EntityRadar::Settings g_scan;
// The finder's place on one side of one room: the next push that way names
// the next way out, until the side, the room or the level changes.
ExitReview::Cycle g_cycle;
Radar::Direction g_cycleSide = Radar::Direction::Up;
ExitReview::Sides g_cycleSides = ExitReview::Sides::Screen;
Rooms::RoomKey g_cycleRoom;
int g_cycleLevel = 0;
uint64_t g_nextPoll = 0;

void Forget() {
	g_rooms.Forget();
	g_cycle.Reset();
	g_cycleRoom = {};
	g_cycleLevel = 0;
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
	const Radar::Position offset{target.x - player.x, target.y - player.y};
	return SpokenDirection::Of(Sides() == ExitReview::Sides::Map ? ExitReview::MapSide(offset) : offset, Strings::Current());
}
// A passage is said by its wall on the map's sides, by its bearing on the screen's.
const char* Where(Radar::Position player, const Rooms::Opening& opening) {
	return Sides() == ExitReview::Sides::Map ? SpokenDirection::Of(ExitReview::Wall(opening), Strings::Current())
		: Where(player, opening.centre);
}
// A finder side in the log, which is English diagnosis, not speech.
const char* SideName(Radar::Direction side) {
	constexpr const char* Names[] = {"up", "left", "right", "down"};
	return Names[static_cast<size_t>(side)];
}
// A way out never taken is tagged: an opening into a chamber never stood in,
// an exit into a level with no room seen yet. `docs/exploration.md`
std::string Tagged(const std::string& label, bool unexplored) {
	if (!unexplored) return label;
	char text[256]{};
	std::snprintf(text, sizeof text, Strings::Current().wayUnexplored, label.c_str());
	return text;
}
bool UnexploredExit(const EntityRadar::Entity& exit) {
	return exit.destinationLevel != 0 && !TerrainService::Known(static_cast<int>(exit.destinationLevel));
}
void TagOpenings(const Radar::World& world, int level, Rooms::Room& room) {
	for (auto& opening : room.openings) {
		const int index = world.RoomAt(static_cast<int>(opening.across.x), static_cast<int>(opening.across.y));
		if (index < 0) continue;
		const auto& across = world.rooms[static_cast<size_t>(index)];
		opening.unexplored = !TerrainService::Visited(level,
			across.x / LevelExitLayout::SubtilesPerTile, across.y / LevelExitLayout::SubtilesPerTile);
	}
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
		std::snprintf(text, sizeof text, strings.roomExitNamed, Tagged(entity.name, UnexploredExit(entity)).c_str(), Where(player, entity.position));
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
		line += Tagged(Where(world.player, opening), opening.unexplored);
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
	TagOpenings(world, state.level, room);
	Announce(state, world, room, "asked");
}
// The level exits standing inside this room.
std::vector<EntityRadar::Entity> ExitsIn(const Lifecycle& state, const Rooms::Room& room) {
	std::vector<EntityRadar::Entity> entities, exits;
	if (!NativeEntities::Read(state.player, state.session, entities)) return exits;
	for (auto& entity : entities)
		if (entity.kind == EntityRadar::Kind::Exit && room.Contains(entity.position)) exits.push_back(std::move(entity));
	return exits;
}
// A plain opening is not a unit guidance can re-read, so it is offered as a
// fixed place: the room kind, which guidance never re-locates.
EntityRadar::Entity Place(const ExitReview::Item& item) {
	EntityRadar::Entity entity;
	entity.kind = EntityRadar::Kind::Room;
	entity.position = item.point;
	entity.name = item.label;
	entity.placeId = NativeLevelExits::Identity(item.point);
	entity.id = static_cast<uint32_t>(entity.placeId) | 1u; // never zero: guidance ignores id 0
	return entity;
}
bool SideOf(InputDirection::StickPush push, Radar::Direction& side) {
	switch (push) {
	case InputDirection::StickPush::Up:    side = Radar::Direction::Up; return true;
	case InputDirection::StickPush::Left:  side = Radar::Direction::Left; return true;
	case InputDirection::StickPush::Right: side = Radar::Direction::Right; return true;
	case InputDirection::StickPush::Down:  side = Radar::Direction::Down; return true;
	default: return false;
	}
}
// One push of the right stick, taken before the level scanner sees it -- but
// only in a dungeon room. Anywhere else the finder does nothing and the push is
// left for the scanner (the user: "no-op the mode outside of a dungeon").
// The way out named is offered to guidance, so BACK+LB or Ctrl+B leads to it.
void Find(const D2RL::PluginContext* context, const Lifecycle& state, uint64_t now) {
	Radar::Direction side = Radar::Direction::Up;
	const auto sides = Sides();
	// On the map's sides the stick is read turned: a flick up-right asks for the northeast wall.
	if (!SideOf(sides == ExitReview::Sides::Map ? InputDirection::PeekTurnedRightStickPush()
		: InputDirection::PeekRightStickPush(), side)) return;
	Radar::World world;
	Rooms::Room room;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)
		|| !Rooms::Describe(world, world.player, room)) return;
	(void)InputDirection::TakeRightStickPush("exit finder");
	TagOpenings(world, state.level, room);
	const auto& strings = Strings::Current();
	const Rooms::RoomKey key{room.x, room.y, true};
	if (!(key == g_cycleRoom) || state.level != g_cycleLevel || side != g_cycleSide || sides != g_cycleSides) {
		g_cycleSides = sides;
		g_cycle.Reset();
		g_cycleRoom = key;
		g_cycleLevel = state.level;
		g_cycleSide = side;
	}
	const auto items = ExitReview::Toward(
		ExitReview::Collect(world.player, room.openings, ExitsIn(state, room), strings, UnexploredExit), world.player, side, sides);
	const size_t index = g_cycle.Next(items);
	if (index == ExitReview::NoItem) {
		const auto text = ExitReview::Nothing(side, strings, sides);
		ScreenReader::Speak(text, true);
		AsyncLog::InfoF(context, "[room] finder %s%s: none of %zu openings -> %s", SideName(side),
			sides == ExitReview::Sides::Map ? " (map)" : "", room.openings.size(), text.c_str());
		return;
	}
	const auto& item = items[index];
	const auto text = ExitReview::Describe(item, g_scan, strings);
	ScreenReader::Speak(text, true);
	BeaconService::Offer(item.named ? item.entity : Place(item), state.level, now);
	AsyncLog::InfoF(context, "[room] finder %s%s: %zu of %zu at=%.0f,%.0f named=%u unexplored=%u -> %s",
		SideName(side), sides == ExitReview::Sides::Map ? " (map)" : "", index + 1, items.size(),
		item.point.x, item.point.y, item.named ? 1u : 0u, item.unexplored ? 1u : 0u, text.c_str());
}
auto FinderToggle(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize)
		|| event->kind != D2RL::Input::ActionEventKind::Pressed) return D2RL::Input::ActionResult::Ignored;
	g_finderToggle = true;
	return D2RL::Input::ActionResult::Handled;
}
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") g_command = Command::On;
	else if (arg == "off") g_command = Command::Off;
	else if (arg == "layout") g_asked = true;
	else if (arg == "finder") g_finderToggle = true;
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
bool Outdoor(int level) {
	return Generated::FindLevelFlags(static_cast<int32_t>(level)).drlgType == OutdoorDrlgType;
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_supported = NativeWorld::Initialize(context) && NativeEntities::Initialize(context);
	(void)context->RegisterConsoleCommand("access-rooms",
		Console, "Room ways out: on|off|status, layout to hear the room you are in, finder to toggle the exit finder. Ctrl+M, Ctrl+V; BACK+Y and BACK+A on the pad.");
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		const D2RL::Input::ActionRegistration actions[]{
			{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = "rooms-toggle", .displayName = "Toggle room exit announcements", .category = "Accessibility",
				.defaultPrimary = {D2RL::Input::Key::M, D2RL::Input::Modifier::Control}, .callback = Toggle},
			{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = "exit-finder-toggle", .displayName = "Toggle the exit finder", .category = "Accessibility",
				.defaultPrimary = {D2RL::Input::Key::V, D2RL::Input::Modifier::Control}, .callback = FinderToggle}};
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
	const bool toggle = InputDirection::TakeBackWith(InputDirection::FaceButton::A) || g_finderToggle.exchange(false);
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
		if (asked || toggle) ScreenReader::Speak(Strings::Current().roomsUnavailable, true);
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
	// BACK+A or Ctrl+V: the exit finder on or off, only in play.
	if (toggle && state.ready && RadarUi::InPlay()) {
		g_finder = !g_finder;
		std::lock_guard lock(g_mutex);
		g_cycle.Reset();
		ScreenReader::Speak(g_finder ? Strings::Current().finderEnabled : Strings::Current().finderDisabled, true);
		AsyncLog::InfoF(context, "[room] exit finder %s", g_finder ? "on" : "off");
	}
	// Peek first: the in-play gate walks the panel tree, and the stick is idle
	// on all but a few ticks. The maze check keeps it off everywhere else.
	if (g_finder && state.ready && Maze(state.level)
		&& InputDirection::PeekRightStickPush() != InputDirection::StickPush::None && RadarUi::InPlay()) {
		std::lock_guard lock(g_mutex);
		Find(context, state, now);
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
	TagOpenings(world, state.level, room);
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
	g_enabled = true; g_asked = false; g_finderToggle = false; g_finder = false; g_mapSides = true; g_supported = false; g_nextPoll = 0;
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
}

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled() { return g_enabled; }
void SetEnabled(bool enabled) { g_command = enabled ? Command::On : Command::Off; }
bool Finder() { return g_finder; }
bool MapSides() { return g_mapSides.load(std::memory_order_relaxed); }
void SetMapSides(bool enabled) { g_mapSides.store(enabled, std::memory_order_relaxed); }
void SetFinder(bool enabled) { if (g_finder != enabled) g_finderToggle = true; }
}
