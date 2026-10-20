#include "beacon_service.h"
#include "async_log.h"
#include "beacon.h"
#include "death_place.h"
#include "beacon_audio.h"
#include "beacon_navigation.h"
#include "crossing_memory.h"
#include "input_direction.h"
#include "keyboard_service.h"
#include "level_areas.h"
#include "map_survey_service.h"
#include "descriptions.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_level_areas.h"
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
#include "route_follow.h"
#include "route_legs.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "spoken_direction.h"
#include "terrain_service.h"
#include "zone_transitions.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <mutex>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace D2RAccess::BeaconService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, Toggle, Clear, Status, Exploration, FullAssistance, SoundsOn, SoundsOff, Route };
// The locked target is re-read on this cadence: fast enough to follow a
// walking monster, far cheaper than the 50 ms UI poll.
constexpr uint64_t UpdateMs = 150;
// A scan result can be locked while it is still what the player heard last.
constexpr uint64_t OfferWindowMs = 15000;
constexpr uint64_t LogIntervalMs = 1000;
// Policy: a target last read this recently counts as reached when it vanishes at arrival.
constexpr uint64_t VanishedOnArrivalMs = 2000;

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
struct Target { EntityRadar::Entity entity; int level = 0; uint64_t offeredMs = 0, seenMs = 0, missingMs = 0; };
Target g_offer, g_target, g_parked;
Lifecycle g_seen;
bool g_supported = false, g_sounds = true;
int g_revision = 0;
uint64_t g_nextUpdate = 0, g_lastLog = 0;
bool g_wasPlaying = false;
// The level's logical rooms are fixed for as long as the level instance is,
// so the graph is built once per instance rather than on a timer. A new
// session or a new level id is a new instance.
RoomGraph::Graph g_graph;
int g_graphLevel = 0;
uint64_t g_graphSession = 0;
// The locked target's way over the remembered map, kept between updates.
BeaconNavigation::RememberedWay g_way;
// How guidance leads, for the log line written when it changes.
enum class Lead { Direct, Rooms, Map, Guess, None };
std::optional<Lead> g_lead;
// The route described last, followed on foot while its level lasts.
constexpr uint64_t FollowMs = 250;
// `ofLock`: the route leads to the locked target, and ends when the lock does.
struct Followed { std::string name; int level = 0; uint64_t session = 0, nextMs = 0; bool ofLock = false; };
Followed g_followed;
RouteFollow::Follower g_follower;
std::atomic<bool> g_routePrompts{true};

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
// assistance does: the user asked for it on 2026-09-26. So does a town NPC listed unseen.
bool Unrestricted(const EntityRadar::Entity& entity) {
	return NavigationMode::FullAssistance() || entity.category == EntityRadar::Category::Player
		|| (MapSurveyService::TownNpcsEverywhere() && entity.kind == EntityRadar::Kind::Npc && entity.placeId);
}
Beacon::Settings SettingsFor(const EntityRadar::Entity& entity) {
	Beacon::Settings settings = g_settings;
	settings.arrivalDistance += HalfFootprint(entity);
	settings.requireKnownRoute = !Unrestricted(entity);
	return settings;
}
// Whether `to` is an area the player walks into from `from` across their shared edge.
bool AcrossBorder(int from, int to) {
	std::vector<NativeLevelAreas::Area> areas;
	if (!NativeLevelAreas::Read(NativeWorld::PlayerRoom(), areas)) return false;
	const auto neighbours = LevelAreas::Neighbours(areas, static_cast<uint32_t>(to));
	return std::any_of(neighbours.begin(), neighbours.end(),
		[from](const LevelAreas::Crossing& crossing) { return crossing.level == static_cast<uint32_t>(from); });
}
// Policy: an area border whose crossing has not been seen is led to anywhere this
// near the middle of its edge, which is often in the trees or cliffs lining it.
// The crossing loads long before (the game keeps two rooms round the player),
// and the target then moves to it.
constexpr int BorderReach = 20;
enum class Border { None, Crossing, Unknown };
// An area border is walked across where both levels' floor meets, rarely at the
// middle of the shared edge that names it: the crossing loaded now, else one seen
// this game (CrossingMemory). docs/beacon.md#area-borders
Border ToCrossing(const Lifecycle& state, const Radar::World& world, EntityRadar::Entity& entity) {
	if (!ZoneTransitions::Crosses(entity)) return Border::None;
	if (NativeLevelExits::Layout layout; NativeEntities::ReadLayout(state.player, true, layout)) {
		const auto crossings = ZoneTransitions::Find(world, layout, Navigation::MaxRadius);
		CrossingMemory::Note(state.session, state.level, crossings);
		if (ZoneTransitions::MoveToCrossing(entity, crossings)) return Border::Crossing;
	}
	if (const auto known = CrossingMemory::Find(state.session, state.level, entity.destinationLevel, world.player)) {
		entity.position = *known;
		return Border::Crossing;
	}
	// A warp unit is where it is; only an edge between two areas is walked across anywhere along it.
	if (entity.unit != EntityRadar::Entity::NoUnit || !AcrossBorder(state.level, static_cast<int>(entity.destinationLevel)))
		return Border::None;
	// The middle of the edge can fall just inside the other area: the nearest cell of this one stands for it.
	const auto* graph = Rooms(state);
	if (graph && NativeRooms::Containing(graph->rooms, entity.position) == NativeRooms::None) {
		double best = std::numeric_limits<double>::infinity();
		const auto from = entity.position;
		for (const auto& room : graph->rooms) {
			const auto bounds = NativeRooms::Subtiles(room);
			const Radar::Position inside{std::clamp(from.x, static_cast<double>(bounds.minX), bounds.maxX - 1.0),
				std::clamp(from.y, static_cast<double>(bounds.minY), bounds.maxY - 1.0)};
			if (const double distance = std::hypot(inside.x - from.x, inside.y - from.y); distance < best) {
				best = distance;
				entity.position = inside;
			}
		}
	}
	return Border::Unknown;
}
// The rooms a way to `entity` may cross in exploration: the explored ones, and
// the target's own room. Nothing when the player stands in no room of the level.
// What the scanner lists has been found, so a way to it can be planned even
// where its room was never stood in (the user, 2026-10-14): an unexplored room
// at the edge of the known map, an exit seen from the doorway, a border crossed
// where a known one is. docs/beacon.md#exploration-mode
std::optional<std::vector<NativeRooms::Room>> ExploredFor(const Lifecycle& state, const RoomGraph::Graph& graph,
	Radar::Position player, const EntityRadar::Entity& entity) {
	if (NativeRooms::Containing(graph.rooms, player) == NativeRooms::None) return std::nullopt;
	auto explored = TerrainService::ExploredRooms(state.level, graph.rooms, player);
	if (!NavigationPolicy::Contains(explored, entity.position))
		if (const size_t room = NativeRooms::Containing(graph.rooms, entity.position); room != NativeRooms::None)
			explored.push_back(graph.rooms[room]);
	return explored;
}
// Where a way to `entity` may end other than round it: a room's own rectangle,
// reached at the first step inside since its centre may be in a wall, or the
// stretch of a border whose crossing is not known yet.
std::optional<LevelRoute::Area> GoalArea(const Lifecycle& state, const EntityRadar::Entity& entity, Border border) {
	if (border == Border::Unknown) return LevelRoute::Area::Around(entity.position, BorderReach);
	const auto* graph = entity.kind == EntityRadar::Kind::Room ? Rooms(state) : nullptr;
	if (!graph) return std::nullopt;
	const auto room = std::find_if(graph->rooms.begin(), graph->rooms.end(), [&](const auto& known) { return known.id == entity.placeId; });
	if (room == graph->rooms.end()) return std::nullopt;
	return LevelRoute::Area::Of(*room);
}
std::optional<BeaconNavigation::Guidance> Guidance(const Lifecycle& state, EntityRadar::Entity entity, uint64_t now,
	BeaconNavigation::RememberedWay& way) {
	const int margin = 1 + static_cast<int>(std::ceil(HalfFootprint(entity)));
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return std::nullopt;
	const auto border = ToCrossing(state, world, entity);
	const Radar::Position target = entity.position;
	const auto* graph = Rooms(state);
	// The walks' own map, learnt from the whole world before exploration masks it.
	const BeaconNavigation::Recall recall{KeyboardService::RememberedMap(world), &way, now, GoalArea(state, entity, border)};
	if (Unrestricted(entity)) return BeaconNavigation::Prepare(std::move(world), target, graph, nullptr, margin, nullptr, recall);
	if (!graph) return std::nullopt;
	auto explored = ExploredFor(state, *graph, world.player, entity);
	if (!explored) return std::nullopt;
	return BeaconNavigation::Prepare(std::move(world), target, graph, &*explored, margin, nullptr, recall);
}
void LogLead(const D2RL::PluginContext* context, const BeaconNavigation::Guidance& guidance) {
	const Lead lead = !guidance.remembered ? (guidance.hasWaypoint ? Lead::Rooms : Lead::Direct)
		: !guidance.hasWaypoint ? Lead::None : guidance.guess ? Lead::Guess : Lead::Map;
	if (g_lead == lead) return;
	g_lead = lead;
	constexpr const char* Names[] = {"direct", "room-graph", "remembered-map", "remembered-map-guess", "no-way"};
	const auto to = guidance.hasWaypoint ? guidance.waypoint : guidance.target;
	AsyncLog::InfoF(context, "[beacon] lead=%s way_cells=%zu to=%.0f,%.0f name=%s", Names[static_cast<int>(lead)],
		guidance.wayCells, to.x, to.y, g_target.entity.name.c_str());
}
// A room is reached by stepping into it, anywhere: its centre may lie inside a
// wall, and what is in it is in sight from its doorway (the user, 2026-10-14).
bool InTargetRoom(const Lifecycle& state, Radar::Position player) {
	if (g_target.entity.kind != EntityRadar::Kind::Room) return false;
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
// The beacon's sounds have a switch of their own. With it off the beacon still
// locks, follows and arrives, in speech alone.
void Cue(BeaconAudio::Sound sound, BeaconAudio::Placement placement = {}) {
	if (g_sounds) BeaconAudio::Play(sound, placement);
}
void Release(const char* reason) {
	if (g_tracker.Locked()) AsyncLog::InfoF(g_context, "[beacon] released reason=%s id=%u name=%s", reason, g_target.entity.id, g_target.entity.name.c_str());
	g_tracker.Clear();
	g_target = {};
	g_way.Clear();
	if (g_follower.Active() && g_followed.ofLock) g_follower.Stop();
	g_lead.reset();
	BeaconAudio::Silence(reason);
}
bool Parked() { return g_parked.entity.id != 0; }
// A lifecycle change ends the lock, except a step into a neighbouring area: a
// route along a border may cross it, so the lock waits there for the way back.
void OnLifecycle(const Lifecycle& state, uint64_t now) {
	const Lifecycle before = std::exchange(g_seen, state);
	g_offer = {};
	const bool walked = before.ready && state.ready && before.session == state.session
		&& before.level && state.level && before.level != state.level;
	if (walked && Parked() && g_parked.level == state.level) {
		g_target = std::exchange(g_parked, {});
		g_target.seenMs = now;
		g_tracker.Lock(now);
		g_nextUpdate = now;
		Cue(BeaconAudio::Sound::Locked);
		AsyncLog::InfoF(g_context, "[beacon] resumed id=%u level=%d name=%s", g_target.entity.id, g_target.level, g_target.entity.name.c_str());
		return;
	}
	g_parked = {};
	const bool parks = walked && g_tracker.Locked() && g_target.level == before.level
		&& g_target.entity.destinationLevel != static_cast<uint32_t>(state.level) && AcrossBorder(before.level, state.level);
	if (parks) g_parked = g_target;
	Release(parks ? "left-area" : "lifecycle");
}
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "lock" || arg == "toggle") g_command = Command::Toggle;
	else if (arg == "clear" || arg == "off") g_command = Command::Clear;
	else if (arg == "status") g_command = Command::Status;
	else if (arg == "exploration") g_command = Command::Exploration;
	else if (arg == "full") g_command = Command::FullAssistance;
	else if (arg == "sounds-on") g_command = Command::SoundsOn;
	else if (arg == "sounds-off") g_command = Command::SoundsOff;
	else if (arg == "route") g_command = Command::Route;
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
	if ((g_tracker.Locked() && !retarget) || (Parked() && !offered)) {
		g_parked = {};
		Release("cancelled");
		Cue(BeaconAudio::Sound::Cleared);
		ScreenReader::Speak(strings.beaconCleared, true);
		return;
	}
	if (!BeaconAudio::Available()) { ScreenReader::Speak(strings.beaconUnavailable, true); return; }
	if (!offered) {
		ScreenReader::Speak(strings.beaconNothing, true);
		return;
	}
	if (!Unrestricted(g_offer.entity)) {
		// Its own way: a refused offer must not cost the locked target its plan.
		BeaconNavigation::RememberedWay probe;
		const auto guidance = Guidance(state, g_offer.entity, now, probe);
		const auto settings = SettingsFor(g_offer.entity);
		const bool known = guidance && guidance->available;
		const auto destination = !known ? g_offer.entity.position : guidance->hasWaypoint ? guidance->waypoint : guidance->target;
		if (!known || !Beacon::Reaches(guidance->world, Beacon::FindRoute(guidance->world, destination, settings),
			destination, settings)) {
			// What the guidance had: a refusal reads the same whether the way was
			// empty, its waypoint off the loaded floor, or the route to it short,
			// and only these fields tell those apart. A synthetic level of the
			// same shape is led across correctly (tests/beacon_guidance_tests.cpp,
			// RoomAcrossTheLevel), so a live refusal needs its own evidence.
			AsyncLog::InfoF(g_context, "[beacon] refused reason=%s lead=%s waypoint=%u guess=%u way_cells=%zu rooms=%zu to=%.0f,%.0f name=%s",
				!guidance ? "unreadable" : !known ? "outside-explored" : "no-route",
				!guidance ? "-" : !guidance->remembered ? "room-graph" : guidance->hasWaypoint ? (guidance->guess ? "remembered-map-guess" : "remembered-map") : "no-way",
				guidance && guidance->hasWaypoint ? 1u : 0u, guidance && guidance->guess ? 1u : 0u,
				guidance ? guidance->wayCells : 0u, guidance ? guidance->roomCount : 0u,
				destination.x, destination.y, g_offer.entity.name.c_str());
			Announce(strings.beaconKnownRouteOnly, g_offer.entity.name);
			return;
		}
	}
	// A refused retarget above keeps the old lock; an accepted one replaces it silently.
	if (retarget) Release("retargeted");
	g_parked = {};
	g_target = g_offer;
	g_target.seenMs = now;
	g_tracker.Lock(now);
	g_nextUpdate = now;
	Cue(BeaconAudio::Sound::Locked);
	Announce(strings.beaconFollowing, g_target.entity.name);
	AsyncLog::InfoF(g_context, "[beacon] locked id=%u kind=%u at=%.0f,%.0f level=%d name=%s", g_target.entity.id,
		static_cast<unsigned>(g_target.entity.kind), g_target.entity.position.x, g_target.entity.position.y,
		g_target.level, g_target.entity.name.c_str());
}
// The target was read for and not found: since when, for Gone().
bool Missing(uint64_t now) {
	if (!g_target.missingMs) g_target.missingMs = now;
	return false;
}
// Re-reads the locked target's current position. A target that is out of the
// loaded rooms keeps its last known place: that is still where it was.
bool Refresh(const Lifecycle& state, uint64_t now) {
	if (g_target.entity.placeId == DeathPlace::Identity) { g_target.seenMs = now; return true; }
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
		if (!NativeGroundItems::ReadLevel(state.player, state.session, snapshot)) return false;
		for (const auto& item : snapshot.items)
			if (item.id == g_target.entity.id) {
				g_target.entity.position = {item.position.x, item.position.y};
				g_target.seenMs = now;
				return true;
			}
		// An item is loaded again under a new id as an object is (MEASURED
		// 2026-09-28): one of the same name where it lay is the one led to. The
		// walk to an item already took it so; the beacon said the item was lost
		// with the item lying there.
		for (const auto& item : snapshot.items)
			if (item.source == GroundItems::SourceKind::Item && item.name == g_target.entity.name
				&& std::abs(item.position.x - g_target.entity.position.x) <= 1 && std::abs(item.position.y - g_target.entity.position.y) <= 1) {
				g_target.entity.id = item.id;
				g_target.entity.position = {item.position.x, item.position.y};
				g_target.seenMs = now;
				return true;
			}
		return Missing(now);
	}
	// A locked exit may be a warp or a boundary with the next area; read both
	// so the beacon can still find the one it is leading to.
	const bool exit = g_target.entity.kind == EntityRadar::Kind::Exit && g_target.entity.placeId != 0;
	// As far as the scanner reads: what it named may be rooms away.
	const bool townNpc = g_target.entity.kind == EntityRadar::Kind::Npc && g_target.entity.placeId != 0;
	if (!NativeEntities::Read(state.player, state.session, entities, exit, exit, true, townNpc)) return false;
	for (const auto& entity : entities)
		if (EntityRadar::SameTarget(entity, g_target.entity)) {
			g_target.entity = entity;
			g_target.seenMs = now;
			return true;
		}
	// An object is loaded again under a new id: it is still the one led to.
	for (const auto& entity : entities)
		if (EntityRadar::Recreated(g_target.entity, entity)) {
			g_target.entity = entity;
			g_target.seenMs = now;
			return true;
		}
	return Missing(now);
}
// A unit read absent from a room the game keeps loaded beside the player -- an item
// another player took, Cain led away -- is gone, not still where it was (a tester,
// 2026-10-15). Exits are not all units, so they are left to arrival.
bool Gone(const Lifecycle& state, Radar::Position player, uint64_t now) {
	if (!g_target.missingMs || now - g_target.seenMs <= VanishedOnArrivalMs || g_target.entity.kind == EntityRadar::Kind::Exit) return false;
	const auto* graph = Rooms(state);
	if (!graph) return false;
	const size_t here = NativeRooms::Containing(graph->rooms, player);
	const size_t there = NativeRooms::Containing(graph->rooms, g_target.entity.position);
	return here != NativeRooms::None && there != NativeRooms::None
		&& (here == there || RoomGraph::Touching(graph->rooms[here], graph->rooms[there]));
}
void Update(const D2RL::PluginContext* context, const Lifecycle& state, uint64_t now) {
	if (!Playing()) return; // dialogue, pause or another window: keep the lock, stay quiet
	const bool seen = Refresh(state, now);
	if (seen) g_target.missingMs = 0;
	const auto prepared = Guidance(state, g_target.entity, now, g_way);
	if (!prepared) return;
	const auto& guidance = *prepared;
	if (!guidance.available) {
		if (!g_tracker.RouteLost(now, g_settings)) return;
		const auto name = g_target.entity.name;
		// Its own reason: the guidance had no way at all, which is a different
		// fault from a way whose proved route stops short, and the two were
		// indistinguishable in the log.
		Release("outside-explored");
		Announce(Strings::Current().beaconKnownRouteEnded, name);
		return;
	}
	const auto& world = guidance.world;
	if (!seen && Gone(state, world.player, now)) {
		const auto name = g_target.entity.name;
		AsyncLog::InfoF(context, "[beacon] gone id=%u name=%s", g_target.entity.id, name.c_str());
		Release("gone");
		Announce(Strings::Current().beaconLost, name);
		return;
	}
	// A target that vanished as the player reached it (a chest opened, an item
	// picked up) was reached; one gone for longer was lost before the player came.
	const bool reached = seen || (g_target.seenMs && now - g_target.seenMs <= VanishedOnArrivalMs);
	const auto arrive = [reached] {
		const auto name = g_target.entity.name;
		Release("arrived");
		Cue(BeaconAudio::Sound::Arrived);
		Announce(reached ? Strings::Current().beaconArrived : Strings::Current().beaconLost, name);
	};
	if (InTargetRoom(state, world.player)) { arrive(); return; }
	const auto* waypoint = guidance.hasWaypoint ? &guidance.waypoint : nullptr;
	const auto update = g_tracker.Tick(world, guidance.target, now, SettingsFor(g_target.entity), waypoint);
	if (!update.valid) return;
	if (update.routeUnavailable) {
		const auto name = g_target.entity.name;
		AsyncLog::InfoF(context, "[beacon] route stopped short: lead=%s waypoint=%u guess=%u way_cells=%zu rooms=%zu to=%.0f,%.0f",
			!guidance.remembered ? "room-graph" : guidance.hasWaypoint ? (guidance.guess ? "remembered-map-guess" : "remembered-map") : "no-way",
			guidance.hasWaypoint ? 1u : 0u, guidance.guess ? 1u : 0u, guidance.wayCells, guidance.roomCount,
			guidance.hasWaypoint ? guidance.waypoint.x : guidance.target.x, guidance.hasWaypoint ? guidance.waypoint.y : guidance.target.y);
		Release("known-route-unavailable");
		Announce(Strings::Current().beaconKnownRouteEnded, name);
		return;
	}
	if (update.arrived) { arrive(); return; }
	LogLead(context, guidance);
	if (update.ping) Cue(BeaconAudio::Sound::Ping, Place(update.heading));
	if (!g_lastLog || now - g_lastLog >= LogIntervalMs) {
		g_lastLog = now;
		AsyncLog::InfoF(context, "[beacon] position=%.0f,%.0f target=%.0f,%.0f seen=%u distance=%.1f remaining=%.1f routed=%u heading=%.2f,%.2f route_cells=%zu name=%s",
			world.player.x, world.player.y, guidance.target.x, guidance.target.y, seen ? 1u : 0u,
			update.distance, update.remaining, update.routed ? 1u : 0u, update.heading.x, update.heading.y,
			g_tracker.CurrentRoute().path.size(), g_target.entity.name.c_str());
		if (waypoint != nullptr && !guidance.remembered) {
			// Room size is the open question the first dungeon answers: the
			// doorway case needs the next room inside the 152-subtile window.
			const auto& room = guidance.waypointRoom;
			AsyncLog::InfoF(context, "[beacon] hop=%.0f,%.0f in_next_room=%u room=%d,%d %dx%d tiles rooms=%zu",
				waypoint->x, waypoint->y, guidance.inNextRoom ? 1u : 0u, room.tileX, room.tileY, room.tileWidth, room.tileHeight,
				guidance.roomCount);
		}
	}
}
// Where a way to `entity` ends: where guidance would say it has arrived, at the
// first step into a room, or near a border whose crossing is not known yet.
LevelRoute::Area WayEnd(const Lifecycle& state, const EntityRadar::Entity& entity, Border border) {
	if (const auto area = GoalArea(state, entity, border)) return *area;
	const int margin = std::max(1 + static_cast<int>(std::ceil(HalfFootprint(entity))),
		static_cast<int>(std::ceil(SettingsFor(entity).arrivalDistance)));
	return LevelRoute::Area::Around(entity.position, margin);
}
// The whole way to what guidance follows, else to what was named last, said as
// legs: what a sighted player plans off the map. docs/route-description.md
void DescribeRoute(const Lifecycle& state, uint64_t now) {
	const auto& strings = Strings::Current();
	EntityRadar::Entity entity;
	if (g_tracker.Locked()) entity = g_target.entity;
	else if (g_offer.entity.id && g_offer.level == state.level) entity = g_offer.entity;
	else { ScreenReader::Speak(strings.beaconNothing, true); return; }
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) {
		AsyncLog::InfoF(g_context, "[route] world unreadable name=%s", entity.name.c_str());
		Announce(strings.routeNone, entity.name);
		return;
	}
	const auto end = WayEnd(state, entity, ToCrossing(state, world, entity));
	const auto place = end.Centre();
	const auto* memory = KeyboardService::RememberedMap(world);
	const auto* graph = Rooms(state);
	const bool restricted = !Unrestricted(entity);
	std::optional<std::vector<NativeRooms::Room>> explored;
	if (restricted && graph) explored = ExploredFor(state, *graph, world.player, entity);
	LevelRoute::Way way;
	if (memory && (!restricted || explored)) {
		BeaconNavigation::RememberedWay planner;
		way = planner.From(*memory, explored ? &*explored : nullptr, world.player, end, world.bodyPattern, now);
	}
	if (way.path.empty()) {
		const bool here = end.Contains(world.player);
		AsyncLog::InfoF(g_context, "[route] none name=%s here=%u map=%u restricted=%u", entity.name.c_str(), here ? 1u : 0u,
			memory ? 1u : 0u, restricted ? 1u : 0u);
		if (!here) { Announce(restricted ? strings.beaconKnownRouteOnly : strings.routeNone, entity.name); return; }
		char text[256]{};
		std::snprintf(text, sizeof text, strings.surveyBearing, entity.name.c_str(), strings.scanAdjacent);
		ScreenReader::Speak(text, true);
		return;
	}
	// The ground as the way knew it: past the explored rooms only guessed at, as ground not seen yet.
	const auto groundAt = [&](int x, int y) {
		const auto ground = memory->At(x, y);
		const bool known = !explored || NavigationPolicy::Contains(*explored, {static_cast<double>(x), static_cast<double>(y)});
		return known || ground == LevelRoute::Ground::Outside ? ground : LevelRoute::Ground::Unseen;
	};
	const auto cell = [](double coordinate) { return static_cast<int>(std::lround(coordinate)); };
	std::vector<size_t> doors;
	for (size_t i = 0; i < way.path.size(); ++i)
		if (groundAt(cell(way.path[i].x), cell(way.path[i].y)) == LevelRoute::Ground::Door) doors.push_back(i);
	// A leg is straightened only over ground the way itself may cross.
	const auto passable = [&](int x, int y) {
		const auto ground = groundAt(x, y);
		return ground == LevelRoute::Ground::Open || ground == LevelRoute::Ground::Door
			|| (way.guess && ground == LevelRoute::Ground::Unseen);
	};
	const auto legs = RouteLegs::Describe(world.player, way.path, doors, passable, SpokenDirection::CurrentStyle());
	const auto text = RouteLegs::Speak(entity.name, legs, way.guess, strings);
	ScreenReader::Speak(text, true);
	AsyncLog::InfoF(g_context, "[route] from=%.0f,%.0f to=%.0f,%.0f cells=%zu doors=%zu legs=%zu guess=%u restricted=%u -> %s",
		world.player.x, world.player.y, place.x, place.y, way.path.size(), doors.size(), legs.size(), way.guess ? 1u : 0u,
		restricted ? 1u : 0u, text.c_str());
	if (!g_routePrompts.load(std::memory_order_relaxed)) return;
	g_followed = {entity.name, state.level, state.session, now, g_tracker.Locked()};
	g_follower.Start(world.player, legs);
}
// The route asked for last, its next leg said at each turn. docs/route-description.md#following-it
void FollowRoute(const Lifecycle& state, uint64_t now) {
	if (!g_follower.Active() || now < g_followed.nextMs) return;
	g_followed.nextMs = now + FollowMs;
	if (!g_routePrompts.load(std::memory_order_relaxed) || !state.ready || state.level != g_followed.level
		|| state.session != g_followed.session) {
		AsyncLog::InfoF(g_context, "[route] follow stopped name=%s", g_followed.name.c_str());
		g_follower.Stop();
		return;
	}
	if (!Playing()) return;
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return;
	const auto legs = g_follower.Legs();
	const auto step = g_follower.Tick(world.player, now);
	const auto& strings = Strings::Current();
	char text[320]{};
	if (step.event == RouteFollow::Event::Turn) {
		std::string leg = RouteLegs::Say(legs[step.leg], strings);
		if (legs[step.leg].door && step.leg + 1 < legs.size()) leg += strings.routeThen + RouteLegs::Say(legs[step.leg + 1], strings);
		std::snprintf(text, sizeof text, strings.routeNow, leg.c_str());
	} else if (step.event == RouteFollow::Event::Off) {
		std::snprintf(text, sizeof text, strings.routeOff, g_followed.name.c_str());
	} else if (step.event == RouteFollow::Event::End && !g_tracker.Locked()) {
		// A locked beacon says its own arrival.
		std::snprintf(text, sizeof text, strings.beaconArrived, g_followed.name.c_str());
	}
	if (step.event != RouteFollow::Event::None)
		AsyncLog::InfoF(g_context, "[route] follow %s leg=%zu at=%.0f,%.0f name=%s -> %s",
			step.event == RouteFollow::Event::Turn ? "turn" : step.event == RouteFollow::Event::Off ? "off" : "end", step.leg,
			world.player.x, world.player.y, g_followed.name.c_str(), text);
	if (*text) ScreenReader::Speak(text, true);
}
}

void Offer(const EntityRadar::Entity& entity, int level, uint64_t now) {
	if (!entity.id) return;
	g_offer = {entity, level, now, now};
}

bool Offered(EntityRadar::Entity& entity, int level) {
	if (!g_offer.entity.id || g_offer.level != level) return false;
	entity = g_offer.entity;
	return true;
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
		// Keyboard equivalent of BACK+LB.
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "beacon-toggle", .displayName = "Follow or release the scanned target", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::B, D2RL::Input::Modifier::Control}, .callback = Action,
			.userData = reinterpret_cast<void*>(Command::Toggle)};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
		// Keyboard equivalent of BACK + right stick down.
		const D2RL::Input::ActionRegistration route{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "route-describe", .displayName = "Describe the route to the scanned target", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::E, D2RL::Input::Modifier::Control}, .callback = Action,
			.userData = reinterpret_cast<void*>(Command::Route)};
		handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &route, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
	}
	(void)context->RegisterConsoleCommand("access-beacon", Console,
		"Target beacon: lock|clear|status|exploration|full|sounds-on|sounds-off|route. BACK+LB or Ctrl+B requests a route; exploration uses visited areas only.");
	const auto tuning = RadarConfig::ReadBeacon(context);
	g_supported = BeaconAudio::Start(context, static_cast<double>(RadarConfig::Read(context).volume) / RadarConfig::MaxVolume,
		RadarConfig::StrengthScale(tuning.panStrength), RadarConfig::StrengthScale(tuning.pitchStrength));
	AsyncLog::InfoF(context, "[beacon] Registered; audio=%u full_assistance=%u.", g_supported ? 1u : 0u,
		NavigationMode::FullAssistance() ? 1u : 0u);
	return true;
}

namespace {
// BACK + right stick up: the hand-written words of the area the player is in (a tester, 2026-10-16).
void DescribeArea(const D2RL::PluginContext* context, int level) {
	const auto name = level > 0 ? NativeEntities::LevelName(static_cast<uint32_t>(level)) : std::string{};
	std::string text = name.empty() ? std::string{} : Descriptions::Find(name);
	if (text.empty()) {
		char none[300]{};
		std::snprintf(none, sizeof none, Strings::Current().noAreaDescription, name.c_str());
		text = none;
	}
	AsyncLog::InfoF(context, "[descriptions] BACK+up in \"%s\" (level %d)", name.c_str(), level);
	ScreenReader::Speak(text, true);
}
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	const bool clicked = InputDirection::TakeBackWithLb();
	const auto chord = InputDirection::TakeBackWithRightStick();
	const uint64_t now = GetTickCount64();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	// Guidance's controls are play's alone: in a menu BACK+LB is the menu's
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
		OnLifecycle(state, now);
	}
	if (command == Command::Exploration || command == Command::FullAssistance) {
		Release("navigation-mode");
		g_offer = g_parked = {};
		NavigationMode::SetFullAssistance(command == Command::FullAssistance);
		g_settings.requireKnownRoute = !NavigationMode::FullAssistance();
		const auto& strings = Strings::Current();
		ScreenReader::Speak(NavigationMode::FullAssistance() ? strings.navigationFullAssistance : strings.navigationExploration, true);
		AsyncLog::InfoF(context, "[beacon] full_assistance=%u", NavigationMode::FullAssistance() ? 1u : 0u);
	}
	if (command == Command::SoundsOn || command == Command::SoundsOff) {
		g_sounds = command == Command::SoundsOn;
		if (!g_sounds) BeaconAudio::Silence("sounds-off");
		const auto& strings = Strings::Current();
		ScreenReader::Speak(g_sounds ? strings.beaconSoundsOn : strings.beaconSoundsOff, true);
		AsyncLog::InfoF(context, "[beacon] sounds=%u", g_sounds ? 1u : 0u);
	}
	if (!inPlay && (clicked || command == Command::Toggle || command == Command::Clear)) {
		AsyncLog::InfoF(context, "[beacon] %s ignored: not in play", clicked ? "BACK+LB" : "command");
	} else if (command == Command::Clear && (g_tracker.Locked() || Parked())) {
		g_parked = {};
		Release("cancelled");
		Cue(BeaconAudio::Sound::Cleared);
		ScreenReader::Speak(Strings::Current().beaconCleared, true);
	} else if (command == Command::Toggle || clicked) {
		Toggle(state, now);
	}
	if (command == Command::Route || chord == InputDirection::StickPush::Down) {
		if (!inPlay) AsyncLog::InfoF(context, "[route] %s ignored: not in play", chord != InputDirection::StickPush::None ? "BACK+down" : "command");
		else DescribeRoute(state, now);
	} else if (chord == InputDirection::StickPush::Up) {
		if (inPlay) DescribeArea(context, state.level);
	} else if (chord != InputDirection::StickPush::None) {
		AsyncLog::InfoF(context, "[route] BACK+right stick %s: nothing bound", InputDirection::PushName(chord));
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
	FollowRoute(state, now);
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
	g_offer = g_target = g_parked = {};
	g_seen = {};
	g_supported = false;
	g_sounds = true;
	g_revision = 0;
	g_nextUpdate = g_lastLog = 0;
	g_wasPlaying = false;
	g_command = Command::None;
	g_graph = {};
	g_graphLevel = 0;
	g_graphSession = 0;
	g_follower.Stop();
	g_followed = {};
	g_routePrompts = true;
	NavigationMode::SetFullAssistance(false);
}

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
void SetFullAssistance(bool enabled) { g_command = enabled ? Command::FullAssistance : Command::Exploration; }
bool Sounds() { return g_sounds; }
bool RoutePrompts() { return g_routePrompts.load(std::memory_order_relaxed); }
void SetRoutePrompts(bool enabled) { g_routePrompts.store(enabled, std::memory_order_relaxed); }
void SetSounds(bool enabled) { g_command = enabled ? Command::SoundsOn : Command::SoundsOff; }
}
