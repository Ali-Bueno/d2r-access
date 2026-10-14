#include "beacon.h"
#include "beacon_navigation.h"
#include "level_fixture.h"
#include "level_route.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace D2RAccess;
using namespace LevelFixture;
using Radar::Position;
using Radar::World;

// Guidance over walkable ground (docs/beacon.md#walkable-guidance): the loaded
// window's own route when it gets there, else as far along the remembered
// map's way as the loaded floor goes, else nothing -- never the straight line.
namespace {
constexpr uint16_t Wall = 0x0001, Door = 0x0800;
constexpr int Margin = 1;

double Dot(Position a, Position b) { return a.x * b.x + a.y * b.y; }
Position Unit(Position from, Position to) {
	const double length = std::hypot(to.x - from.x, to.y - from.y);
	return length > 0 ? Position{(to.x - from.x) / length, (to.y - from.y) / length} : Position{};
}
// World axes, not the screen's.
const Position MinusY{0, -1}, PlusX{1, 0}, PlusY{0, 1};

LevelRoute::Memory Seen(const Level& level) {
	LevelRoute::Memory memory;
	Check(memory.Begin(1, 1, level.Rooms()), "fixture: the map begins");
	for (int row = 0; row < Down; ++row)
		for (int column = 0; column < Across; ++column)
			(void)memory.Learn(level.Loaded({Left + column * RoomCells + 5.0, Top + row * RoomCells + 5.0}, 0));
	return memory;
}
Beacon::Update Follow(Beacon::Tracker& tracker, const BeaconNavigation::Guidance& guidance, uint64_t now, const Beacon::Settings& s) {
	return tracker.Tick(guidance.world, guidance.target, now, s, guidance.hasWaypoint ? &guidance.waypoint : nullptr);
}
// The heading once the spoken name has had its time.
Beacon::Update Settled(const BeaconNavigation::Guidance& guidance, const Beacon::Settings& s) {
	Beacon::Tracker tracker;
	tracker.Lock(0);
	(void)Follow(tracker, guidance, 0, s);
	return Follow(tracker, guidance, s.lockDelayMs, s);
}

// "56 steps right" through a wall: the way is down, right and up, all of it loaded.
void AroundTheWall() {
	Level level;
	level.Fill(Left + 100, Top, Left + 102, Top + 159, Wall); // open below Top + 160
	const auto graph = RoomGraph::Build(level.Rooms());
	auto memory = Seen(level);
	const Position player{Left + 80.0, Top + 50.0}, target{Left + 120.0, Top + 50.0};
	const World world = level.Loaded(player, Down);
	BeaconNavigation::RememberedWay way;
	const auto guidance = BeaconNavigation::Prepare(world, target, &graph, nullptr, Margin, nullptr, {&memory, &way, 0});
	Check(guidance.available && !guidance.hasWaypoint && !guidance.remembered, "a target the loaded floor reaches is routed to on it");
	const Beacon::Settings s;
	const auto update = Settled(guidance, s);
	Check(update.routed && update.ping, "the ping follows the route round the wall");
	Check(Dot(update.heading, PlusY) > 0.5 && Dot(update.heading, Unit(player, target)) < 0.9,
		"it points down along the wall, not through it at the target");
}

// Seen shut all round: nothing is guessed and nothing is played.
void NoKnownRoute() {
	Level level;
	level.Fill(Left + 100, Top, Left + 102, Top + Height - 1, Wall);
	const auto graph = RoomGraph::Build(level.Rooms());
	auto memory = Seen(level);
	const Position player{Left + 80.0, Top + 50.0}, target{Left + 120.0, Top + 50.0};
	BeaconNavigation::RememberedWay way;
	const auto guidance = BeaconNavigation::Prepare(level.Loaded(player, Down), target, &graph, nullptr, Margin, nullptr, {&memory, &way, 0});
	Check(guidance.available && guidance.remembered && !guidance.hasWaypoint && guidance.wayCells == 0,
		"the remembered map knows no way, and no waypoint is guessed in its place");
	Beacon::Settings full;
	Beacon::Tracker following;
	following.Lock(0);
	bool silent = true, kept = true;
	for (uint64_t now = 0; now <= full.lockDelayMs + 2 * full.routeLossMs; now += 150) {
		const auto update = Follow(following, guidance, now, full);
		silent = silent && !update.ping && !update.routed && update.heading.x == 0 && update.heading.y == 0;
		kept = kept && update.valid && !update.routeUnavailable;
	}
	Check(silent, "with no walkable route no ping sounds: never the straight line through the wall");
	Check(kept, "full assistance keeps the lock, silent, until a route is known");
	Beacon::Settings exploring;
	exploring.requireKnownRoute = true;
	Beacon::Tracker known;
	known.Lock(0);
	(void)Follow(known, guidance, 0, exploring);
	Check(Follow(known, guidance, exploring.routeLossMs, exploring).routeUnavailable,
		"exploration ends the lock once no route has been known for the whole grace");
}

// The target is beyond the loaded window, behind a wall the player saw earlier.
void BeyondTheWindow() {
	Level level;
	level.Fill(Left, Top + 100, Left + 159, Top + 102, Wall); // the only gap is at the far right
	const auto graph = RoomGraph::Build(level.Rooms());
	auto memory = Seen(level);
	const Position target{Left + 20.0, Top + 20.0};
	Position player{Left + 20.0, Top + 180.0};
	const World first = level.Loaded(player, 1);
	const auto guess = BeaconNavigation::Prepare(first, target, &graph);
	Check(guess.hasWaypoint && Dot(Unit(player, guess.waypoint), MinusY) > 0.5,
		"fixture: the room graph alone heads up, into the dead end under the wall");
	BeaconNavigation::RememberedWay way;
	const auto guidance = BeaconNavigation::Prepare(first, target, &graph, nullptr, Margin, nullptr, {&memory, &way, 0});
	Check(guidance.remembered && guidance.hasWaypoint && !guidance.guess && guidance.wayCells > 250,
		"the remembered map leads the long way round, all of it seen");
	Check(Dot(Unit(player, guidance.waypoint), PlusX) > 0.9, "its waypoint lies along the way, toward the gap");
	const Beacon::Settings s;
	const auto update = Settled(guidance, s);
	Check(update.routed && update.ping && Dot(update.heading, PlusX) > 0.9, "the ping turns away from the target, along the way");
	Beacon::Settings exploring;
	exploring.requireKnownRoute = true;
	Check(Beacon::Reaches(guidance.world, Beacon::FindRoute(guidance.world, guidance.waypoint, exploring), guidance.waypoint, exploring),
		"exploration accepts the lock: the route to the waypoint is proved");

	// Walking it: the window slides, and the guidance never sends the player under the wall.
	Beacon::Tracker tracker;
	tracker.Lock(0);
	bool routed = true, deadEnd = false, arrived = false;
	int ticks = 0;
	for (uint64_t now = 2000; ticks < 100 && !arrived; ++ticks, now += 2000) {
		const World world = level.Loaded(player, 1);
		(void)memory.Learn(world);
		const auto step = BeaconNavigation::Prepare(world, target, &graph, nullptr, Margin, nullptr, {&memory, &way, now});
		const auto followed = Follow(tracker, step, now, s);
		if (followed.arrived) { arrived = true; break; }
		routed = routed && followed.routed;
		const auto route = Beacon::FindRoute(step.world, step.hasWaypoint ? step.waypoint : target, s);
		Check(!route.path.empty(), "a route to where guidance leads is always found");
		player = route.path[std::min<size_t>(7, route.path.size() - 1)];
		deadEnd = deadEnd || (player.x < Left + 130 && player.y > Top + 102 && player.y < Top + RoomCells * 3);
	}
	Check(arrived && ticks <= 60, "the walk arrives by the gap, in about the way's own length");
	Check(routed, "every update on the way follows a proved route");
	Check(!deadEnd, "and none of them leads into the dead end under the wall");
}

// Exploration plans over explored rooms alone, so the shortcut through an
// unexplored room is not given away.
void ExploredOnly() {
	Level level;
	level.Fill(Left, Top + 100, Left + 159, Top + 102, Wall);
	level.Fill(Left + 5, Top + 100, Left + 15, Top + 102, 0); // a gap straight above the player, in room (0, 2)
	const auto rooms = level.Rooms();
	const auto graph = RoomGraph::Build(rooms);
	auto memory = Seen(level);
	const Position player{Left + 20.0, Top + 180.0}, target{Left + 20.0, Top + 20.0};
	const auto unexplored = NativeRooms::Subtiles(rooms[2 * Across]);
	std::vector<NativeRooms::Room> explored = rooms;
	explored.erase(explored.begin() + 2 * Across);
	const World world = level.Loaded(player, 1);

	BeaconNavigation::RememberedWay whole;
	const auto everywhere = BeaconNavigation::Prepare(world, target, &graph, nullptr, Margin, nullptr, {&memory, &whole, 0});
	Check(everywhere.hasWaypoint && Dot(Unit(player, everywhere.waypoint), MinusY) > 0.9, "fixture: over the whole map the way is the gap straight above");
	BeaconNavigation::RememberedWay known;
	const auto exploring = BeaconNavigation::Prepare(world, target, &graph, &explored, Margin, nullptr, {&memory, &known, 0});
	Check(exploring.available && exploring.hasWaypoint && Dot(Unit(player, exploring.waypoint), PlusX) > 0.9,
		"exploration leads round by the explored gap instead");
	const auto path = BeaconNavigation::RememberedWay().From(memory, &explored, player, target, Margin, 1, 0).path;
	bool inside = false;
	for (const auto point : path) inside = inside || NativeRooms::Contains(unexplored, static_cast<int>(point.x), static_cast<int>(point.y));
	Check(!path.empty() && !inside, "no cell of the explored way is in the unexplored room");
}

// A room is reached at its first step inside: its centre can be wall (the
// user, 2026-10-14: a route to the unexplored room next door did not work).
void RoomWithAWalledCentre() {
	Level level;
	const auto rooms = level.Rooms();
	const auto& target = rooms[2 * Across + 3];
	const auto bounds = NativeRooms::Subtiles(target);
	level.Fill(bounds.minX + 10, bounds.minY + 10, bounds.maxX - 11, bounds.maxY - 11, Wall);
	const auto graph = RoomGraph::Build(rooms);
	auto memory = Seen(level);
	const Position player{Left + 2 * RoomCells + 20.0, Top + 2 * RoomCells + 20.0};
	const Position centre{(bounds.minX + bounds.maxX - 1) / 2.0, (bounds.minY + bounds.maxY - 1) / 2.0};
	// The player's room and the target's own, as exploration admits it.
	const std::vector<NativeRooms::Room> explored{rooms[2 * Across + 2], target};
	BeaconNavigation::RememberedWay toCentre;
	const auto point = BeaconNavigation::Prepare(level.Loaded(player, 1), centre, &graph, &explored, Margin, nullptr, {&memory, &toCentre, 0});
	Check(!point.hasWaypoint, "fixture: no way ends at the walled centre");
	BeaconNavigation::RememberedWay toRoom;
	const auto room = BeaconNavigation::Prepare(level.Loaded(player, 1), centre, &graph, &explored, Margin, nullptr,
		{&memory, &toRoom, 0, LevelRoute::Area::Of(target)});
	Check(room.available && room.hasWaypoint && room.waypoint.x == bounds.minX
		&& NativeRooms::Contains(bounds, static_cast<int>(room.waypoint.x), static_cast<int>(room.waypoint.y)),
		"the room's way ends at its first step inside");
	const auto update = Settled(room, Beacon::Settings{});
	Check(update.routed && Dot(update.heading, PlusX) > 0.9, "and the ping leads straight into it");
	const Position inside{bounds.minX + 2.0, player.y};
	BeaconNavigation::RememberedWay there;
	const auto arrived = BeaconNavigation::Prepare(level.Loaded(inside, 1), centre, &graph, &explored, Margin, nullptr,
		{&memory, &there, 0, LevelRoute::Area::Of(target)});
	Check(arrived.hasWaypoint && arrived.waypoint.x == inside.x && arrived.waypoint.y == inside.y,
		"inside it, where the player stands is the place");
}

// A target past a room never stood in: the way guesses across it, and that room
// is followed into as the target's own is (the user, 2026-10-14: what the
// scanner lists can be routed to).
void PastAnUnexploredRoom() {
	Level level;
	const auto rooms = level.Rooms();
	const auto graph = RoomGraph::Build(rooms);
	auto memory = Seen(level);
	const Position player{Left + 20.0, Top + 2 * RoomCells + 20.0};
	const Position target{Left + 2 * RoomCells + 20.0, Top + 2 * RoomCells + 20.0};
	const std::vector<NativeRooms::Room> explored{rooms[2 * Across], rooms[2 * Across + 2]};
	BeaconNavigation::RememberedWay way;
	const auto guidance = BeaconNavigation::Prepare(level.Loaded(player, 1), target, &graph, &explored, Margin, nullptr, {&memory, &way, 0});
	const auto middle = NativeRooms::Subtiles(rooms[2 * Across + 1]);
	Check(guidance.available && guidance.remembered && guidance.guess, "the way guesses across the room never stood in");
	Check(guidance.hasWaypoint && NativeRooms::Contains(middle, static_cast<int>(guidance.waypoint.x), static_cast<int>(guidance.waypoint.y)),
		"and leads into it, as far as its floor is loaded");
	Check(Settled(guidance, Beacon::Settings{}).routed, "the ping follows a proved route there");
}

// A shut door is opened, not walked round: guidance leads up to it.
void DoorOnTheWay() {
	Level level;
	level.Fill(Left, Top + 100, Left + Width - 1, Top + 102, Wall);
	level.Fill(Left + 19, Top + 100, Left + 21, Top + 102, Door);
	const auto graph = RoomGraph::Build(level.Rooms());
	auto memory = Seen(level);
	const Position target{Left + 20.0, Top + 20.0};
	Check(LevelRoute::Plan().From(memory, {Left + 20.0, Top + 180.0}, target, Margin, 1).path.empty(),
		"fixture: a walk plans no way through the shut door");
	BeaconNavigation::RememberedWay way;
	const Position far{Left + 20.0, Top + 180.0};
	const auto approach = BeaconNavigation::Prepare(level.Loaded(far, 1), target, &graph, nullptr, Margin, nullptr, {&memory, &way, 0});
	Check(approach.hasWaypoint && Dot(Unit(far, approach.waypoint), MinusY) > 0.99, "guidance plans through the door, straight toward it");
	const Position near{Left + 20.0, Top + 130.0};
	const auto atDoor = BeaconNavigation::Prepare(level.Loaded(near, 1), target, &graph, nullptr, Margin, nullptr, {&memory, &way, 0});
	Check(atDoor.hasWaypoint && atDoor.waypoint.x == Left + 20 && atDoor.waypoint.y > Top + 102 && atDoor.waypoint.y <= Top + 105,
		"with the door loaded and shut the waypoint is the floor in front of it");
	const auto update = Settled(atDoor, Beacon::Settings{});
	Check(update.routed && Dot(update.heading, MinusY) > 0.9, "and the ping leads there, for the player to open it");
}

// The remembered way is planned again at a bounded rate.
void BoundedReplanning() {
	Level level;
	auto memory = Seen(level);
	const auto rooms = level.Rooms();
	const Position player{Left + 20.0, Top + 180.0}, place{Left + 180.0, Top + 20.0};
	const auto crosses = [](const std::vector<Position>& path) {
		for (const auto point : path)
			if (point.y >= Top + 100 && point.y <= Top + 102 && point.x >= Left + 60) return true;
		return false;
	};
	BeaconNavigation::RememberedWay way;
	const uint64_t start = 5000;
	Check(crosses(way.From(memory, nullptr, player, place, Margin, 1, start).path), "fixture: the first way runs straight across the open level");
	level.Fill(Left + 60, Top + 100, Left + Width - 1, Top + 102, Wall);
	for (int column = 0; column < Across; ++column) (void)memory.Learn(level.Loaded({Left + column * RoomCells + 5.0, Top + 2 * RoomCells + 5.0}, 0));
	const auto refresh = BeaconNavigation::RememberedWay::RefreshMs;
	Check(crosses(way.From(memory, nullptr, player, place, Margin, 1, start + refresh - 1).path),
		"a map that learnt more is not planned on again before RefreshMs");
	Check(!crosses(way.From(memory, nullptr, player, place, Margin, 1, start + refresh).path), "and is once it has passed");
	std::vector<NativeRooms::Room> explored = rooms;
	explored.erase(explored.begin() + 2 * Across, explored.begin() + 2 * Across + 2); // the rooms the way now goes round by
	const auto restricted = way.From(memory, &explored, player, place, Margin, 1, start + refresh + 1);
	Check(restricted.guess, "other rooms to keep to are planned on at once: past them the way only guesses");
	const auto at = [&](Position moved, uint64_t now) {
		const auto path = way.From(memory, nullptr, player, moved, Margin, 1, now).path;
		return path.empty() ? Position{} : path.back();
	};
	const uint64_t later = start + 2 * refresh + 2;
	const auto first = at(place, later);
	const double drift = BeaconNavigation::RememberedWay::PlaceDrift;
	const auto near = at({place.x - drift / 2, place.y}, later + 1);
	Check(near.x == first.x && near.y == first.y, "a place that moved a little is planned to where it was");
	const auto moved = at({place.x - 2 * drift, place.y}, later + 2);
	Check(std::abs(moved.x - (place.x - 2 * drift)) <= Margin, "one that moved farther is planned to where it is");
}
}

int main() {
	AroundTheWall();
	NoKnownRoute();
	BeyondTheWindow();
	ExploredOnly();
	RoomWithAWalledCentre();
	PastAnUnexploredRoom();
	DoorOnTheWay();
	BoundedReplanning();
	Passed();
}
