#include "beacon_service.h"
#include "async_log.h"
#include "beacon.h"
#include "beacon_audio.h"
#include "beacon_navigation.h"
#include "input_direction.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_level_exits.h"
#include "native_rooms.h"
#include "native_world.h"
#include "navigation_grid.h"
#include "navigation_mode.h"
#include "navigation_policy.h"
#include "pad_filter.h"
#include "room_graph.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "terrain_service.h"
#include "zone_transitions.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::BeaconService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, Toggle, Clear, Status, Exploration, FullAssistance };
// The locked target is re-read on this cadence: fast enough to follow a
// walking monster, far cheaper than the 50 ms UI poll.
constexpr uint64_t UpdateMs = 150;
// A scan result can be locked while it is still what the player heard last.
constexpr uint64_t OfferWindowMs = 15000;
constexpr uint64_t LogIntervalMs = 1000;

struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = 0, revision = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
Beacon::Settings g_settings;
Beacon::Tracker g_tracker;
struct Target { EntityRadar::Entity entity; int level = 0; uint64_t offeredMs = 0, seenMs = 0; };
Target g_offer, g_target;
bool g_supported = false;
int g_revision = 0;
uint64_t g_nextUpdate = 0, g_lastLog = 0;
bool g_wasPlaying = false;
// The level's logical rooms are fixed for as long as the level instance is,
// so the graph is built once per instance rather than on a timer. A new
// session or a new level id is a new instance.
RoomGraph::Graph g_graph;
int g_graphLevel = 0;
uint64_t g_graphSession = 0;

// The room graph for the level being played, or nothing when the level's
// rooms cannot be read. Never a stale level's graph.
const RoomGraph::Graph* Rooms(const Lifecycle& state) {
	if (g_graphLevel != state.level || g_graphSession != state.session || g_graph.Empty()) {
		std::vector<NativeRooms::Room> rooms;
		g_graph = NativeRooms::Read(NativeWorld::PlayerRoom(), rooms) ? RoomGraph::Build(rooms) : RoomGraph::Graph{};
		g_graphLevel = state.level;
		g_graphSession = state.session;
	}
	return g_graph.Empty() ? nullptr : &g_graph;
}
// Nothing when the world or its rooms cannot be read right now: a failed read
// is not a lost route, so a locked beacon waits instead of releasing.
// A large object is reached from beside its native footprint, not at its centre.
double HalfFootprint(const EntityRadar::Entity& entity) {
	const auto side = std::max(entity.footprint.width, entity.footprint.height);
	return side > 1 ? (side - 1) / 2.0 : 0;
}
// Following another player leads through rooms this player has not explored, as full
// assistance does: the user asked for it on 2026-09-26.
bool Unrestricted(const EntityRadar::Entity& entity) {
	return NavigationMode::FullAssistance() || entity.category == EntityRadar::Category::Player;
}
Beacon::Settings SettingsFor(const EntityRadar::Entity& entity) {
	Beacon::Settings settings = g_settings;
	settings.arrivalDistance += HalfFootprint(entity);
	settings.requireKnownRoute = !Unrestricted(entity);
	return settings;
}
// An area border is walked across where both levels' floor meets, rarely at the
// middle of the shared edge that names it. docs/beacon.md#area-borders
bool AtCrossing(const Lifecycle& state, const Radar::World& world, EntityRadar::Entity& entity) {
	NativeLevelExits::Layout layout;
	return ZoneTransitions::Crosses(entity) && NativeEntities::ReadLayout(state.player, true, layout)
		&& ZoneTransitions::MoveToCrossing(entity, ZoneTransitions::Find(world, layout, Navigation::MaxRadius));
}
std::optional<BeaconNavigation::Guidance> Guidance(const Lifecycle& state, EntityRadar::Entity entity) {
	const int margin = 1 + static_cast<int>(std::ceil(HalfFootprint(entity)));
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return std::nullopt;
	const bool crossing = AtCrossing(state, world, entity);
	const Radar::Position target = entity.position;
	const auto* graph = Rooms(state);
	if (Unrestricted(entity)) return BeaconNavigation::Prepare(std::move(world), target, graph, nullptr, margin);
	if (!graph) return std::nullopt;
	const size_t current = NativeRooms::Containing(graph->rooms, world.player);
	if (current == NativeRooms::None) return std::nullopt;
	auto explored = TerrainService::ExploredRooms(state.level, graph->rooms, world.player);
	// The room holding the target may be entered unexplored, never crossed: an
	// unexplored room at the edge of the known map, or where a known border is crossed.
	const bool enterable = entity.category == EntityRadar::Category::UnexploredRoom || crossing;
	if (enterable && !NavigationPolicy::Contains(explored, target))
		if (const size_t room = NativeRooms::Containing(graph->rooms, target); room != NativeRooms::None)
			explored.push_back(graph->rooms[room]);
	return BeaconNavigation::Prepare(std::move(world), target, graph, &explored, margin);
}
// An unexplored room is reached by stepping into it: its centre may lie inside a wall.
bool InTargetRoom(const Lifecycle& state, Radar::Position player) {
	if (g_target.entity.category != EntityRadar::Category::UnexploredRoom) return false;
	const auto* graph = Rooms(state);
	const size_t room = graph ? NativeRooms::Containing(graph->rooms, player) : NativeRooms::None;
	return room != NativeRooms::None && graph->rooms[room].id == g_target.entity.placeId;
}
bool Playing() {
	return RadarUi::InPlay();
}
// Screen placement of a world direction, the ground-item loops' convention:
// native projection 0x334DB0 turns the world diagonals into the screen axes.
BeaconAudio::Placement Place(Beacon::Position heading) {
	constexpr double Diagonal = std::numbers::sqrt2 / 2;
	return {(heading.x - heading.y) * Diagonal, -(heading.x + heading.y) * Diagonal};
}
void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) g_state.level = event->currentValue;
	++g_state.revision;
}
void Release(const char* reason) {
	if (g_tracker.Locked()) AsyncLog::InfoF(g_context, "[beacon] released reason=%s id=%u name=%s", reason, g_target.entity.id, g_target.entity.name.c_str());
	g_tracker.Clear();
	g_target = {};
	BeaconAudio::Silence(reason);
}
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "lock" || arg == "toggle") g_command = Command::Toggle;
	else if (arg == "clear" || arg == "off") g_command = Command::Clear;
	else if (arg == "status") g_command = Command::Status;
	else if (arg == "exploration") g_command = Command::Exploration;
	else if (arg == "full") g_command = Command::FullAssistance;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* data) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed)
		return D2RL::Input::ActionResult::Ignored;
	g_command = static_cast<Command>(reinterpret_cast<uintptr_t>(data));
	return D2RL::Input::ActionResult::Handled;
}
void Announce(const char* format, const std::string& name) {
	char text[256]{};
	std::snprintf(text, sizeof text, format, name.c_str());
	ScreenReader::Speak(text, true);
}
bool SameOfferedTarget() {
	return EntityRadar::SameTarget(g_offer.entity, g_target.entity);
}
// A press on what is already followed releases it; on a newly scanned target it
// moves the beacon there in one press. docs/beacon.md
void Toggle(const Lifecycle& state, uint64_t now) {
	const auto& strings = Strings::Current();
	const bool offered = g_offer.entity.id && g_offer.level == state.level
		&& now - g_offer.offeredMs <= OfferWindowMs;
	const bool retarget = g_tracker.Locked() && offered && !SameOfferedTarget();
	if (g_tracker.Locked() && !retarget) {
		Release("cancelled");
		BeaconAudio::Play(BeaconAudio::Sound::Cleared, {});
		ScreenReader::Speak(strings.beaconCleared, true);
		return;
	}
	if (!BeaconAudio::Available()) { ScreenReader::Speak(strings.beaconUnavailable, true); return; }
	if (!offered) {
		ScreenReader::Speak(strings.beaconNothing, true);
		return;
	}
	const auto guidance = Guidance(state, g_offer.entity);
	const auto settings = SettingsFor(g_offer.entity);
	if (!Unrestricted(g_offer.entity)) {
		const bool known = guidance && guidance->available;
		const auto destination = !known ? g_offer.entity.position : guidance->hasWaypoint ? guidance->waypoint : guidance->target;
		if (!known || !Beacon::Reaches(guidance->world, Beacon::FindRoute(guidance->world, destination, settings),
			destination, settings)) {
			AsyncLog::InfoF(g_context, "[beacon] refused reason=%s name=%s",
				!guidance ? "unreadable" : !known ? "outside-explored" : "no-route", g_offer.entity.name.c_str());
			Announce(strings.beaconKnownRouteOnly, g_offer.entity.name);
			return;
		}
	}
	// A refused retarget above keeps the old lock; an accepted one replaces it silently.
	if (retarget) Release("retargeted");
	g_target = g_offer;
	g_target.seenMs = now;
	g_tracker.Lock(now);
	g_nextUpdate = now;
	BeaconAudio::Play(BeaconAudio::Sound::Locked, {});
	Announce(strings.beaconFollowing, g_target.entity.name);
	AsyncLog::InfoF(g_context, "[beacon] locked id=%u kind=%u at=%.0f,%.0f level=%d name=%s", g_target.entity.id,
		static_cast<unsigned>(g_target.entity.kind), g_target.entity.position.x, g_target.entity.position.y,
		g_target.level, g_target.entity.name.c_str());
}
// Re-reads the locked target's current position. A target that is out of the
// loaded rooms keeps its last known place: that is still where it was.
bool Refresh(const Lifecycle& state, uint64_t now) {
	std::vector<EntityRadar::Entity> entities;
	// A room is not a native unit and never moves: it is exactly where the
	// catalogue said it was, so there is nothing to re-read and nothing that
	// could be reported lost on arrival.
	if (g_target.entity.kind == EntityRadar::Kind::Room) {
		g_target.seenMs = now;
		return true;
	}
	if (g_target.entity.kind == EntityRadar::Kind::Item) {
		GroundItems::Snapshot snapshot;
		if (!NativeGroundItems::Read(state.player, state.session, snapshot)) return false;
		for (const auto& item : snapshot.items)
			if (item.id == g_target.entity.id) {
				g_target.entity.position = {item.position.x, item.position.y};
				g_target.seenMs = now;
				return true;
			}
		return false;
	}
	// A locked exit may be a warp or a boundary with the next area; read both
	// so the beacon can still find the one it is leading to.
	const bool exit = g_target.entity.kind == EntityRadar::Kind::Exit && g_target.entity.placeId != 0;
	if (!NativeEntities::Read(state.player, state.session, entities, exit, exit)) return false;
	for (const auto& entity : entities)
		if (EntityRadar::SameTarget(entity, g_target.entity)) {
			g_target.entity = entity;
			g_target.seenMs = now;
			return true;
		}
	return false;
}
void Update(const D2RL::PluginContext* context, const Lifecycle& state, uint64_t now) {
	if (!Playing()) return; // dialogue, pause or another window: keep the lock, stay quiet
	const bool seen = Refresh(state, now);
	const auto prepared = Guidance(state, g_target.entity);
	if (!prepared) return;
	const auto& guidance = *prepared;
	if (!guidance.available) {
		if (!g_tracker.RouteLost(now, g_settings)) return;
		const auto name = g_target.entity.name;
		Release("known-route-unavailable");
		Announce(Strings::Current().beaconKnownRouteEnded, name);
		return;
	}
	const auto& world = guidance.world;
	const auto arrive = [seen] {
		const auto name = g_target.entity.name;
		Release("arrived");
		BeaconAudio::Play(BeaconAudio::Sound::Arrived, {});
		Announce(seen ? Strings::Current().beaconArrived : Strings::Current().beaconLost, name);
	};
	if (InTargetRoom(state, world.player)) { arrive(); return; }
	const auto* waypoint = guidance.hasWaypoint ? &guidance.waypoint : nullptr;
	const auto update = g_tracker.Tick(world, guidance.target, now, SettingsFor(g_target.entity), waypoint);
	if (!update.valid) return;
	if (update.routeUnavailable) {
		const auto name = g_target.entity.name;
		Release("known-route-unavailable");
		Announce(Strings::Current().beaconKnownRouteEnded, name);
		return;
	}
	if (update.arrived) { arrive(); return; }
	if (update.ping) BeaconAudio::Play(BeaconAudio::Sound::Ping, Place(update.heading));
	if (!g_lastLog || now - g_lastLog >= LogIntervalMs) {
		g_lastLog = now;
		AsyncLog::InfoF(context, "[beacon] position=%.0f,%.0f target=%.0f,%.0f seen=%u distance=%.1f remaining=%.1f routed=%u heading=%.2f,%.2f route_cells=%zu name=%s",
			world.player.x, world.player.y, guidance.target.x, guidance.target.y, seen ? 1u : 0u,
			update.distance, update.remaining, update.routed ? 1u : 0u, update.heading.x, update.heading.y,
			g_tracker.CurrentRoute().path.size(), g_target.entity.name.c_str());
		if (waypoint != nullptr) {
			// Room size is the open question the first dungeon answers: the
			// doorway case needs the next room inside the 152-subtile window.
			const auto& room = guidance.waypointRoom;
			AsyncLog::InfoF(context, "[beacon] hop=%.0f,%.0f in_next_room=%u room=%d,%d %dx%d tiles rooms=%zu",
				waypoint->x, waypoint->y, guidance.inNextRoom ? 1u : 0u, room.tileX, room.tileY, room.tileWidth, room.tileHeight,
				guidance.roomCount);
		}
	}
}
}

void Offer(const EntityRadar::Entity& entity, int level, uint64_t now) {
	if (!entity.id) return;
	g_offer = {entity, level, now, now};
}

bool Register(const D2RL::PluginContext* context) {
	g_context = context;
	NavigationMode::SetFullAssistance(RadarConfig::ReadNavigation(context).fullAssistance);
	g_settings.requireKnownRoute = !NavigationMode::FullAssistance();
	if (!context || context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) { Unregister(); return false; }
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		// Keyboard equivalent of the left-stick click, for testing without a pad.
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "beacon-toggle", .displayName = "Follow or release the scanned target", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::B, D2RL::Input::Modifier::Control}, .callback = Action,
			.userData = reinterpret_cast<void*>(Command::Toggle)};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
	}
	(void)context->RegisterConsoleCommand("access-beacon", Console,
		"Target beacon: lock|clear|status|exploration|full. L3 or Ctrl+B requests a route; exploration uses visited areas only.");
	g_supported = BeaconAudio::Start(context, static_cast<double>(RadarConfig::Read(context).volume) / RadarConfig::MaxVolume);
	AsyncLog::InfoF(context, "[beacon] Registered; audio=%u full_assistance=%u.", g_supported ? 1u : 0u,
		NavigationMode::FullAssistance() ? 1u : 0u);
	return true;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	const bool clicked = InputDirection::TakeLeftStickClick();
	const uint64_t now = GetTickCount64();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	// Guidance's controls are play's alone: in a menu the L3 click is the menu's
	// and guidance ignores it, as it ignores Ctrl+B and the console's lock and
	// clear (the user, 2026-09-21: "ignore guidance while menu is open").
	const bool playing = Playing();
	const bool inPlay = state.ready && playing;
	if (g_wasPlaying && !playing) BeaconAudio::Silence("ui-blocked");
	g_wasPlaying = playing;
	// The mod's BACK combinations are play's alone; in a menu the game has them.
	PadFilter::Claim(context, inPlay);
	if (state.revision != g_revision) {
		g_revision = state.revision;
		Release("lifecycle");
		g_offer = {};
	}
	if (command == Command::Exploration || command == Command::FullAssistance) {
		Release("navigation-mode");
		g_offer = {};
		NavigationMode::SetFullAssistance(command == Command::FullAssistance);
		g_settings.requireKnownRoute = !NavigationMode::FullAssistance();
		const auto& strings = Strings::Current();
		ScreenReader::Speak(NavigationMode::FullAssistance() ? strings.navigationFullAssistance : strings.navigationExploration, true);
		AsyncLog::InfoF(context, "[beacon] full_assistance=%u", NavigationMode::FullAssistance() ? 1u : 0u);
	}
	if (!inPlay && (clicked || command == Command::Toggle || command == Command::Clear)) {
		AsyncLog::InfoF(context, "[beacon] %s ignored: not in play", clicked ? "L3" : "command");
	} else if (command == Command::Clear && g_tracker.Locked()) {
		Release("cancelled");
		BeaconAudio::Play(BeaconAudio::Sound::Cleared, {});
		ScreenReader::Speak(Strings::Current().beaconCleared, true);
	} else if (command == Command::Toggle || clicked) {
		Toggle(state, now);
	}
	if (command == Command::Status) {
		const auto& strings = Strings::Current();
		std::string text = NavigationMode::FullAssistance() ? strings.navigationFullAssistance : strings.navigationExploration;
		text += ". ";
		if (!g_tracker.Locked()) text += strings.beaconNothing;
		else {
			char following[256]{};
			std::snprintf(following, sizeof following, strings.beaconFollowing, g_target.entity.name.c_str());
			text += following;
		}
		ScreenReader::Speak(text, true);
	}
	if (!g_tracker.Locked() || !state.ready || now < g_nextUpdate) return;
	g_nextUpdate = now + UpdateMs;
	Update(context, state, now);
}

void Unregister() {
	Release("unregister");
	BeaconAudio::Stop();
	if (g_context && g_lifecycle) for (const auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	if (g_context && g_input) for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_listeners.clear();
	g_actions.clear();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	g_context = nullptr;
	g_lifecycle = nullptr;
	g_input = nullptr;
	g_offer = g_target = {};
	g_supported = false;
	g_revision = 0;
	g_nextUpdate = g_lastLog = 0;
	g_wasPlaying = false;
	g_command = Command::None;
	g_graph = {};
	g_graphLevel = 0;
	g_graphSession = 0;
	NavigationMode::SetFullAssistance(false);
}
}
