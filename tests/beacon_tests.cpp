#include "beacon.h"
#include "test_support.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using namespace D2RAccess;
using Radar::Layer;
using Radar::Position;
using Radar::State;
using Radar::World;

namespace {
// One resident room, large enough to hold the widest search box the default
// settings can ask for (2*searchRadius+1 cells a side, centred on the player),
// so nothing below is decided by the edge of the fixture instead of by the
// collision it describes.
constexpr int Size = 240;
constexpr uint16_t PlayerPath = 0x1C09; // observed native player walking mask
constexpr uint16_t Wall = 1;            // a walking-mask bit: real collision
constexpr uint16_t Blank = 0x20;        // native unknown subtile, as radar_grid.cpp reads it
const double Sqrt2 = std::numbers::sqrt2;
const Beacon::Settings Default{};

World Field(Position player, int pattern = 0, uint16_t fill = 0) {
	World world;
	world.session = world.level = 1;
	world.player = player;
	world.bodyPattern = pattern;
	world.walkingMask = PlayerPath;
	world.rooms = {{0, 0, Size, Size, std::vector<uint16_t>(static_cast<size_t>(Size) * Size, fill)}};
	return world;
}
void Put(World& world, int x, int y, uint16_t flags) {
	auto& room = world.rooms.front();
	room.cells[static_cast<size_t>(y - room.y) * room.width + (x - room.x)] = flags;
}
void Fill(World& world, int x0, int y0, int x1, int y1, uint16_t flags) { // inclusive
	for (int y = y0; y <= y1; ++y)
		for (int x = x0; x <= x1; ++x) Put(world, x, y, flags);
}
void Column(World& world, int x, uint16_t flags) {
	for (int y = 0; y < Size; ++y) Put(world, x, y, flags);
}
Position At(double x, double y) { return {x, y}; }
double Dot(Position a, Position b) { return a.x * b.x + a.y * b.y; }
double Length(Position p) { return std::hypot(p.x, p.y); }
Position Minus(Position a, Position b) { return {a.x - b.x, a.y - b.y}; }
Position Unit(Position p) { const double l = Length(p); return l > 0 ? Position{p.x / l, p.y / l} : Position{}; }
bool Same(Position a, Position b) { return a.x == b.x && a.y == b.y; }
int Chebyshev(Position a, Position b) {
	return static_cast<int>(std::max(std::llround(std::abs(a.x - b.x)), std::llround(std::abs(a.y - b.y))));
}
// Eight-connected cost with sqrt2 diagonals: the metric the route itself uses.
double Octile(Position a, Position b) {
	const double dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
	return std::max(dx, dy) - std::min(dx, dy) + Sqrt2 * std::min(dx, dy);
}
State Footprint(const World& world, Position cell) {
	return world.Body(static_cast<int>(std::lround(cell.x)), static_cast<int>(std::lround(cell.y)), Layer::Terrain);
}
// Every route is a chain of single native steps over cells the body fits in.
void CheckWalkable(const World& world, const Beacon::Route& route, const char* what) {
	Position previous = world.player;
	bool steps = true, open = true, mapped = true;
	for (const auto cell : route.path) {
		steps &= Chebyshev(previous, cell) == 1;
		open &= Footprint(world, cell) == State::Open;
		mapped &= world.Cell(static_cast<int>(cell.x), static_cast<int>(cell.y), Layer::Terrain) != State::Unknown;
		previous = cell;
	}
	Check(steps, "every route is a chain of single native steps from the player's own cell");
	Check(open, what);
	Check(mapped, "no route is proved through ground no resident room describes");
}
// True when the straight line to the target crosses collision, which is what
// makes a routed heading different from a bearing.
bool StraightLineBlocked(const World& world, Position target) {
	const Position line = Minus(target, world.player);
	const int samples = static_cast<int>(std::ceil(Length(line) * 4));
	for (int i = 0; i <= samples; ++i) {
		const double part = static_cast<double>(i) / samples;
		const Position point{world.player.x + line.x * part, world.player.y + line.y * part};
		if (Footprint(world, point) == State::Blocked) return true;
	}
	return false;
}

void PingCadence() {
	const Beacon::Settings s;
	Check(Beacon::PingIntervalMs(0, s) == s.nearPingMs, "a reached target pings at the near cadence");
	Check(Beacon::PingIntervalMs(-5, s) == s.nearPingMs, "a negative remaining never pings faster than the near cadence");
	Check(Beacon::PingIntervalMs(s.farDistance, s) == s.farPingMs, "the far distance pings at the far cadence");
	Check(Beacon::PingIntervalMs(s.farDistance * 4, s) == s.farPingMs, "past the far distance the cadence stops slowing");
	const double near = static_cast<double>(s.nearPingMs), far = static_cast<double>(s.farPingMs);
	Check(Beacon::PingIntervalMs(s.farDistance / 2, s) == static_cast<uint64_t>(std::llround((near + far) / 2)),
		"half the far distance pings at the arithmetic mean of the two bounds");
	uint64_t previous = 0;
	bool monotone = true, slowed = false;
	for (int step = 0; step <= 200; ++step) {
		const double remaining = step * s.farDistance / 100.0; // 0 .. twice the far distance
		const uint64_t interval = Beacon::PingIntervalMs(remaining, s);
		monotone &= interval >= previous;
		slowed |= interval > previous;
		previous = interval;
		const double part = std::min(remaining / s.farDistance, 1.0);
		Check(interval == static_cast<uint64_t>(std::llround(near + part * (far - near))),
			"the cadence is the linear interpolation between its own two bounds");
		Check(interval >= s.nearPingMs && interval <= s.farPingMs, "no cadence leaves the configured bounds");
	}
	Check(monotone && slowed, "the cadence never speeds up as the remaining route grows");
	Beacon::Settings degenerate;
	degenerate.farDistance = 0;
	Check(Beacon::PingIntervalMs(0, degenerate) == degenerate.nearPingMs
		&& Beacon::PingIntervalMs(1, degenerate) == degenerate.farPingMs,
		"a zero far distance degenerates to the two bounds instead of dividing by zero");
	Beacon::Settings flat;
	flat.nearPingMs = flat.farPingMs = 250;
	Check(Beacon::PingIntervalMs(7, flat) == 250, "equal bounds give one fixed cadence at every distance");
}

void OpenField() {
	const Beacon::Settings s;
	// A pure world diagonal: every minimum-step route is that diagonal, so the
	// proved route and its heading are decided by geometry alone.
	auto world = Field({70, 70});
	constexpr int Reach = 40;
	const Position target = At(70 + Reach, 70 + Reach);
	const auto route = Beacon::FindRoute(world, target, s);
	Check(route.known && route.reachable, "an open field proves a route to the target itself");
	Check(Same(route.path.back(), target), "a reachable route ends on the target's own cell");
	Check(route.path.size() == Reach, "the diagonal route takes one step per cell of displacement");
	Check(std::abs(route.length - Reach * Sqrt2) < 1e-9, "a diagonal route costs sqrt2 per step");
	Check(std::abs(route.length - Octile(world.player, target)) < 1, "the proved length is within a cell of the diagonal distance");
	const Position bearing = Unit(Minus(target, world.player));
	Check(std::abs(route.heading.x - bearing.x) < 1e-9 && std::abs(route.heading.y - bearing.y) < 1e-9,
		"with nothing in the way the heading is the straight line to the target");
	Check(std::abs(Length(route.heading) - 1) < 1e-12, "the heading is a unit vector");
	// The aim point: the first route cell at least lookAhead cells along, which
	// on a diagonal is lookAhead/sqrt2 steps.
	const int aim = static_cast<int>(std::ceil(s.lookAhead / Sqrt2));
	Check(Same(route.path[static_cast<size_t>(aim) - 1], At(world.player.x + aim, world.player.y + aim)),
		"the aimed-at cell is the first one past the look-ahead distance");
	Check(route.length > s.lookAhead, "the route carries on past the cell the heading aims at");
	CheckWalkable(world, route, "every cell of an open-field route is open");

	// A pure world axis: each step has to advance one cell along it, so the
	// proved length is the step count itself.
	auto axis = Field({70, 70});
	const auto east = Beacon::FindRoute(axis, At(70 + Reach, 70), s);
	Check(east.reachable && std::abs(east.length - Reach) < 1e-9, "an axis route in the open costs one cell per step");
	Check(std::abs(Dot(east.heading, {1, 0}) - 1) < 1e-9, "the heading down an open axis is that axis");

	// Any bearing: whatever minimum-step route the search picks, every cell of
	// it lies on a shortest route and the heading stays inside the 45 degree
	// cone the look-ahead aim point can reach.
	for (int dy = -30; dy <= 30; dy += 5) {
		auto open = Field({70, 110});
		const Position goal = At(110, 110 + dy);
		const auto any = Beacon::FindRoute(open, goal, s);
		Check(any.reachable && Same(any.path.back(), goal), "every open-field target is reached whatever its bearing");
		Check(static_cast<int>(any.path.size()) == Chebyshev(open.player, goal), "the route takes exactly the Chebyshev number of steps");
		bool shortest = true;
		for (const auto cell : any.path)
			shortest &= Chebyshev(open.player, cell) + Chebyshev(cell, goal) == Chebyshev(open.player, goal);
		Check(shortest, "every route cell lies on a shortest step route to the target");
		Check(Dot(any.heading, Unit(Minus(goal, open.player))) >= Radar::Diagonal, "the open-field heading holds the bearing of the target");
		CheckWalkable(open, any, "every cell of an open-field route at any bearing is open");
	}

	// The search box is bounded: a target beyond it cannot be proved, and the
	// route stops on the edge of the box that was searched.
	auto distant = Field({110, 110});
	const auto capped = Beacon::FindRoute(distant, At(110 + s.searchRadius + 20, 110), s);
	Check(capped.known && !capped.reachable, "a target past the search box is never claimed reachable");
	Check(Dot(capped.heading, {1, 0}) > 0.9, "an unproved far target is still walked toward");
	Check(capped.path.back().x == 110 + s.searchRadius, "the route stops on the edge of the search box");

	// Degenerate: standing on the target leaves the search nothing to improve.
	auto here = Field({110, 110});
	const auto none = Beacon::FindRoute(here, {110, 110}, s);
	Check(!none.known && !none.reachable && none.path.empty(), "standing on the target proves no first leg");
	Check(none.heading.x == 0 && none.heading.y == 0, "an unproved route carries no heading");
}

// An L corridor. Leg A runs `legA` cells along world X from the player, leg B
// runs `legB` cells along world Y from the corner, and the target sits near the
// end of leg B: the straight line to it leaves the corridor through rock, so a
// bearing would point at a wall and only a proved route turns the corner.
struct Elbow {
	World world;
	Position player, corner, target, axisA, axisB;
};
Elbow MakeElbow(int sx, int sy, int legA = 20, int legB = 70, int pattern = 1) {
	const Position player{110, 110};
	const int cornerX = 110 + legA * sx, endY = 110 + legB * sy;
	Elbow elbow{Field(player, pattern, Wall), player, At(cornerX, 110), At(cornerX, endY - 2 * sy),
		At(sx, 0), At(0, sy)};
	// Five cells wide: the native cross body keeps one clear row on each side.
	Fill(elbow.world, std::min(110, cornerX) - 2, 108, std::max(110, cornerX) + 2, 112, 0);
	Fill(elbow.world, cornerX - 2, std::min(110, endY) - 2, cornerX + 2, std::max(110, endY) + 2, 0);
	return elbow;
}
void Corners() {
	const Beacon::Settings s;
	for (const int sx : {1, -1}) {
		for (const int sy : {1, -1}) {
			const auto elbow = MakeElbow(sx, sy);
			const World& world = elbow.world;
			const Position bearing = Unit(Minus(elbow.target, world.player));
			Check(StraightLineBlocked(world, elbow.target), "the straight line to a target around the corner really does cross collision");
			Check(Dot(elbow.axisA, bearing) < 0.5, "the corridor the player stands in points well away from that bearing");
			const auto route = Beacon::FindRoute(world, elbow.target, s);
			Check(route.known && route.reachable, "a target around a corner is proved reachable through the corridor");
			Check(Same(route.path.back(), elbow.target), "the proved route ends on the target around the corner");
			CheckWalkable(world, route, "every cell of a corridor route is open for the native body");
			Check(Dot(route.heading, elbow.axisA) > 0.9, "the heading points along the corridor the player is standing in");
			Check(Dot(route.heading, bearing) < 0.5, "the heading is not the blocked straight-line bearing");
			bool turns = false;
			for (const auto cell : route.path)
				turns |= std::abs(cell.x - elbow.corner.x) <= 2 && std::abs(cell.y - elbow.corner.y) <= 2;
			Check(turns, "the route goes through the corner instead of through the rock");
			const double straight = Length(Minus(elbow.target, world.player));
			Check(route.length > straight, "going around the corner is longer than the blocked straight line");
			Check(route.length < 2 * straight, "the detour is a route around the corner, not a wander");

			// Approaching the corner: once it is inside the look-ahead window
			// the heading already carries the far leg's direction.
			auto near = elbow;
			near.world.player = At(elbow.corner.x - 4 * sx, 110);
			const auto turning = Beacon::FindRoute(near.world, elbow.target, s);
			Check(turning.known, "a route is still proved from beside the corner");
			Check(Dot(turning.heading, elbow.axisB) > 0.4, "the heading turns the corner before the player reaches it");
			Check(Dot(turning.heading, elbow.axisA) > 0, "turning early never points back down the corridor");
		}
	}
}

void Doorway() {
	const Beacon::Settings s;
	// One wall, one doorway offset sideways from the straight line.
	for (const int offset : {-15, -8, -3, 0, 3, 8, 15}) {
		auto world = Field({70, 110});
		Column(world, 90, Wall);
		const Position door = At(90, 110 + offset);
		Put(world, 90, 110 + offset, 0);
		const Position target{110, 110};
		Check(offset == 0 || StraightLineBlocked(world, target), "an offset doorway leaves the straight line blocked");
		const auto route = Beacon::FindRoute(world, target, s);
		Check(route.known && route.reachable, "a doorway anywhere in the wall keeps the target reachable");
		Check(Same(route.path.back(), target), "the route past a doorway still ends on the target");
		bool through = false, elsewhere = false;
		for (const auto cell : route.path) {
			through |= Same(cell, door);
			elsewhere |= cell.x == 90 && !Same(cell, door);
		}
		Check(through, "the proved route passes through the doorway cell itself");
		Check(!elsewhere, "no route cell crosses the wall anywhere but the doorway");
		CheckWalkable(world, route, "every cell of a route through a doorway is open");
		Check(route.length >= Octile(world.player, door) + Octile(door, target) - 1e-9,
			"the route is at least as long as the two legs through the doorway");
		Check(Dot(route.heading, Unit(Minus(door, world.player))) >= Radar::Diagonal,
			"the heading holds the bearing of the doorway the route has to use");
		// Shutting that one door disconnects the target again.
		Put(world, 90, 110 + offset, Wall);
		const auto shut = Beacon::FindRoute(world, target, s);
		Check(shut.known && !shut.reachable, "closing the only doorway leaves a heading but no proved arrival");
		Check(shut.path.back().x < 90, "with the door shut the route stops on the player's side of the wall");
	}
}

void SealedTarget() {
	const Beacon::Settings s;
	// A walled-off room: the target is mapped, open, and has no way in.
	auto world = Field({70, 110});
	const Position target{110, 110};
	Fill(world, 106, 106, 114, 114, Wall);
	Fill(world, 107, 107, 113, 113, 0);
	Check(world.Cell(110, 110, Layer::Terrain) == State::Open, "the sealed room's own floor is open collision");
	const auto route = Beacon::FindRoute(world, target, s);
	Check(route.known && !route.reachable, "a sealed target is never claimed reachable");
	Check(!route.path.empty(), "an unreachable target still proves a first leg");
	const Position bearing = Unit(Minus(target, world.player));
	Check(Dot(route.heading, bearing) > 0, "the heading still points into the half plane of the target");
	Check(Same(route.path.back(), At(105, 110)), "the route ends on the reachable cell nearest the target");
	Check(std::abs(Length(Minus(route.path.back(), target)) - 5) < 1e-9, "that cell is the wall's own distance from the target");
	bool inside = false;
	for (const auto cell : route.path) inside |= cell.x >= 106 && cell.x <= 114 && cell.y >= 106 && cell.y <= 114;
	Check(!inside, "no route cell is proved inside the sealed room");
	CheckWalkable(world, route, "every cell of a route toward a sealed target is open");

	// A target standing on collision (a chest, a post): the route ends beside it.
	auto solid = Field({70, 110});
	Put(solid, 110, 110, Wall);
	const auto beside = Beacon::FindRoute(solid, target, s);
	Check(beside.known && !beside.reachable, "a target on a blocked cell is not itself reachable");
	Check(Length(Minus(beside.path.back(), target)) <= Sqrt2 + 1e-9, "the route ends on a cell touching the blocked target");
	Check(Dot(beside.heading, bearing) > 0.9, "the heading toward a blocked target is still its bearing");

	// A player who cannot step at all proves nothing, and the tracker says so.
	auto trapped = Field({110, 110}, 0, Wall);
	Put(trapped, 110, 110, 0);
	const auto nothing = Beacon::FindRoute(trapped, At(150, 110), s);
	Check(!nothing.known && !nothing.reachable && nothing.path.empty(), "a player with nowhere to step proves no route at all");
	Beacon::Tracker tracker;
	tracker.Lock(0);
	const auto update = tracker.Tick(trapped, At(150, 110), 0, s);
	Check(update.valid && !update.routed, "the tracker still follows a locked target with no route");
	Check(std::abs(update.heading.x - 1) < 1e-12 && update.heading.y == 0, "with no route the heading falls back to the straight line");
	Check(std::abs(update.remaining - update.distance) < 1e-12, "an unrouted target's remaining distance is the straight line");
}

void UnknownGround() {
	const Beacon::Settings s;
	// No resident room at all past x = 100: the target's own ground is not
	// loaded, which is the ordinary case for a level exit across the map.
	auto edge = Field({60, 110});
	edge.rooms.front().width = 100;
	edge.rooms.front().cells.assign(100 * Size, 0);
	const Position target{140, 110};
	Check(edge.Cell(120, 110, Layer::Terrain) == State::Unknown, "the ground toward that target really is unloaded");
	const auto route = Beacon::FindRoute(edge, target, s);
	Check(route.known && !route.reachable, "a target on unloaded ground is never claimed reachable");
	Check(Same(route.path.back(), At(99, 110)), "the route runs to the loaded cell nearest the target");
	Check(Dot(route.heading, Unit(Minus(target, edge.player))) > 0.9, "the heading still points at the frontier nearest the target");
	CheckWalkable(edge, route, "no route cell lies on ground no room describes");

	// Mapped but blank subtiles: unknown is not a wall and not a floor, and is
	// never routed through even when it is the only way to the target.
	auto blank = Field({110, 110});
	Fill(blank, 140, 0, 142, Size - 1, Blank);
	const Position beyond{170, 110};
	Check(blank.Cell(141, 110, Layer::Terrain) == State::Unknown, "a blank subtile reads as unknown, not as collision");
	const auto stopped = Beacon::FindRoute(blank, beyond, s);
	Check(stopped.known && !stopped.reachable, "a band of unknown subtiles is never routed through");
	Check(Same(stopped.path.back(), At(139, 110)), "the route stops on the last mapped cell before the unknown band");
	CheckWalkable(blank, stopped, "no route cell lies on an unknown subtile");

	// One mapped, open channel through the same band is used, which proves the
	// band itself was what stopped the route.
	Fill(blank, 140, 110, 142, 110, 0);
	const auto threaded = Beacon::FindRoute(blank, beyond, s);
	Check(threaded.reachable && Same(threaded.path.back(), beyond), "a mapped channel through the unknown band is routed through");
	bool channel = false;
	for (const auto cell : threaded.path) channel |= Same(cell, At(141, 110));
	Check(channel, "the route uses the one mapped cell of that channel");
	CheckWalkable(blank, threaded, "every cell of a route threading unknown ground is mapped and open");

	// Blanking the whole band back out takes the arrival away again.
	Fill(blank, 140, 0, 142, Size - 1, Blank);
	Check(!Beacon::FindRoute(blank, beyond, s).reachable, "unknown ground closing over the channel withdraws the proved arrival");
}

void BodyFootprint() {
	const Beacon::Settings s;
	const Position player{70, 110}, target{110, 110};
	for (int pattern = 0; pattern <= 5; ++pattern) {
		const bool square = pattern != 0 && pattern % 2 == 0;
		auto world = Field(player, pattern);
		Column(world, 90, Wall);
		Put(world, 90, 110, 0); // a one-cell hole
		const auto point = Beacon::FindRoute(world, target, s);
		Check(point.reachable == (pattern == 0), "only the native point footprint is routed through a one-cell hole");
		Check((Footprint(world, At(90, 110)) == State::Open) == (pattern == 0),
			"the route's verdict on that hole is the native footprint's own verdict");
		Check(point.known, "a body that does not fit still proves a heading toward the hole");

		Fill(world, 90, 109, 90, 111, 0); // three cells tall
		const auto tall = Beacon::FindRoute(world, target, s);
		Check(tall.reachable, "every native footprint is routed through a three-cell hole in a one-cell wall");
		Check(Footprint(world, At(90, 110)) == State::Open, "the native body fits in the three-cell hole");
		CheckWalkable(world, tall, "every cell of a route through a hole is open for the native body");

		Put(world, 91, 109, Wall); // a corner bump only the square footprint touches
		const auto bumped = Beacon::FindRoute(world, target, s);
		Check(bumped.reachable == !square, "the square footprint is stopped by the corner bump the cross ignores");
		Check((Footprint(world, At(90, 110)) == State::Open) == !square,
			"that verdict is the native square/cross dispatch, not a rule of the beacon's own");
	}
}

void WalkStability() {
	const Beacon::Settings s;
	// A straight corridor: the aim point can sit at most one row off the axis,
	// so the heading can wobble by at most that angle and consecutive headings
	// by at most twice it.
	const double reach = std::ceil(s.lookAhead / Sqrt2); // shortest step count that covers the look-ahead
	const double wobble = std::atan(1.0 / reach);
	auto world = Field({50, 110}, 1, Wall);
	Fill(world, 30, 108, 200, 112, 0);
	const Position target{140, 110}; // inside the search box from every cell walked below
	Position previous{};
	bool steady = true, aligned = true;
	for (int x = 50; x <= 120; ++x) {
		world.player = At(x, 110);
		const auto route = Beacon::FindRoute(world, target, s);
		Check(route.known && route.reachable, "the target stays proved from every cell of the corridor");
		aligned &= Dot(route.heading, {1, 0}) >= std::cos(wobble) - 1e-9;
		if (x > 50) steady &= Dot(route.heading, previous) >= std::cos(2 * wobble) - 1e-9;
		previous = route.heading;
	}
	Check(aligned, "walking a straight corridor holds the heading on the corridor axis");
	Check(steady, "the heading never flaps between one cell of walking and the next");

	// The same walk into a corner: the heading may swing round the corner, but
	// it must do it progressively, never flipping between two cells.
	const auto elbow = MakeElbow(1, -1);
	auto corner = elbow.world;
	bool smooth = true;
	previous = {};
	for (int x = 110; x <= 128; ++x) { // the corridor's own cells, up to the corner
		corner.player = At(x, 110);
		const auto route = Beacon::FindRoute(corner, elbow.target, s);
		Check(route.known, "a route is proved from every cell on the way to the corner");
		if (x > 110) smooth &= Dot(route.heading, previous) >= std::cos(std::numbers::pi / 4);
		previous = route.heading;
	}
	Check(smooth, "the heading swings round a corner by less than a right angle per cell walked");
}

void TrackerLifecycle() {
	const Beacon::Settings s;
	auto world = Field({110, 110});
	const Position target{160, 110};
	Beacon::Tracker tracker;
	Check(!tracker.Locked(), "a fresh tracker is not locked");
	Check(!tracker.Tick(world, target, 1000, s).valid, "an unlocked tracker reports nothing to play");
	Check(!tracker.Tick(world, target, 1000, s).ping, "an unlocked tracker never pings");
	tracker.Lock(1000);
	Check(tracker.Locked(), "locking a target arms the tracker");
	bool early = false, valid = true;
	for (uint64_t now = 1000; now < 1000 + s.lockDelayMs; now += Radar::PollMs) {
		const auto update = tracker.Tick(world, target, now, s);
		early |= update.ping;
		valid &= update.valid;
	}
	Check(valid, "every tick of a locked target is a valid update");
	Check(!early, "no ping sounds while the spoken target name is still being read");
	const auto first = tracker.Tick(world, target, 1000 + s.lockDelayMs, s);
	Check(first.ping, "the first ping sounds as soon as the lock delay has passed");
	Check(first.routed && first.remaining > 0, "the first ping already follows a proved route");
	Check(std::abs(first.distance - 50) < 1e-9, "the update reports the straight-line distance to the target");
	Check(first.remaining >= first.distance - 1e-9, "a proved route is never shorter than the straight line");
	tracker.Clear();
	Check(!tracker.Locked(), "clearing the lock disarms the tracker");
	Check(!tracker.Tick(world, target, 2000, s).valid, "a cleared tracker reports nothing to play");
	Check(tracker.CurrentRoute().path.empty() && !tracker.CurrentRoute().known, "clearing forgets the proved route as well");

	// Arrival is the straight-line distance against the configured threshold.
	Beacon::Tracker arriving;
	arriving.Lock(0);
	bool arrival = true;
	for (int step = 32; step >= 0; --step) {
		auto near = Field({110, 110});
		const double gap = step * 0.25;
		near.player = At(target.x - gap, target.y);
		arrival &= arriving.Tick(near, target, 10000 + static_cast<uint64_t>(step), s).arrived == (gap <= s.arrivalDistance);
	}
	Check(arrival, "arrival is announced inside the arrival distance and never before it");
}

void TrackerCadence() {
	const Beacon::Settings s;
	auto world = Field({110, 110});
	const Position target{180, 110};
	Beacon::Tracker tracker;
	tracker.Lock(0);
	std::vector<uint64_t> pings;
	std::vector<double> remaining;
	for (uint64_t now = 0; now <= 40000; now += Radar::PollMs) {
		world.player = At(std::min(178.0, 110.0 + static_cast<double>(now) / 500.0), 110); // one cell every half second
		const auto update = tracker.Tick(world, target, now, s);
		if (!update.ping) continue;
		pings.push_back(now);
		remaining.push_back(update.remaining);
	}
	Check(pings.size() >= 4, "walking a long way in produces a whole sequence of pings");
	Check(pings.front() >= s.lockDelayMs, "the sequence still starts after the lock delay");
	bool shrinking = true, bounded = true, closing = true;
	for (size_t i = 1; i < pings.size(); ++i) {
		const uint64_t gap = pings[i] - pings[i - 1], older = i > 1 ? pings[i - 1] - pings[i - 2] : gap;
		shrinking &= gap <= older;
		bounded &= gap >= s.nearPingMs && gap <= s.farPingMs + Radar::PollMs;
		closing &= remaining[i] <= remaining[i - 1] + 1e-9;
	}
	Check(closing, "the remaining route shortens as the player walks in");
	Check(shrinking, "consecutive ping gaps shrink as the target gets closer");
	Check(bounded, "every ping gap stays inside the configured cadence bounds");
	Check(pings.back() - pings[pings.size() - 2] < pings[1] - pings[0], "the last gap is faster than the first");

	// Remaining is the proved route when there is one, the straight line when
	// there is not: a detour is spoken as the longer distance it really is.
	auto bent = MakeElbow(1, -1).world;
	const Position around = MakeElbow(1, -1).target;
	bent.player = At(110, 110);
	Beacon::Tracker corner;
	corner.Lock(0);
	const auto routed = corner.Tick(bent, around, s.lockDelayMs, s);
	Check(routed.routed && routed.remaining > routed.distance, "a corner route pings the route length, not the bearing distance");
	Check(Beacon::PingIntervalMs(routed.remaining, s) >= Beacon::PingIntervalMs(routed.distance, s),
		"the longer route slows the cadence instead of hurrying a player round a corner");
}

void TrackerReroute() {
	const Beacon::Settings s;
	auto world = Field({110, 110});
	const Position target{170, 110};
	Beacon::Tracker tracker;
	tracker.Lock(0);
	const uint64_t frozen = 5000; // one instant: only movement can force a new search
	(void)tracker.Tick(world, target, frozen, s);
	const Position first = tracker.CurrentRoute().path.front();
	Check(Chebyshev(world.player, first) == 1, "the route starts on a cell the player can step onto");
	world.player.x += s.reroute - 1;
	(void)tracker.Tick(world, target, frozen, s);
	Check(Same(tracker.CurrentRoute().path.front(), first), "a step shorter than the reroute distance keeps the proved route");
	world.player.x += 1;
	(void)tracker.Tick(world, target, frozen, s);
	const Position second = tracker.CurrentRoute().path.front();
	Check(!Same(second, first) && Chebyshev(world.player, second) == 1,
		"moving the reroute distance proves a new route from where the player now stands");
	const Position moved = At(target.x, target.y + s.reroute);
	(void)tracker.Tick(world, moved, frozen, s);
	Check(Same(tracker.CurrentRoute().path.back(), moved), "a target that moves the reroute distance is routed to afresh");

	// Standing still, the route is refreshed on its own interval: a door that
	// opens is picked up on the next interval, not on the next tick.
	auto shut = Field({110, 110});
	Column(shut, 130, Wall);
	Beacon::Tracker still;
	still.Lock(0);
	const uint64_t start = 1000;
	Check(!still.Tick(shut, target, start, s).routed || !still.CurrentRoute().reachable, "a walled-off target is not reachable through the wall");
	Put(shut, 130, 110, 0);
	(void)still.Tick(shut, target, start + s.routeIntervalMs - Radar::PollMs, s);
	Check(!still.CurrentRoute().reachable, "a route is not searched again on every tick of a standing player");
	(void)still.Tick(shut, target, start + s.routeIntervalMs, s);
	Check(still.CurrentRoute().reachable, "the route interval picks the opened door up without the player moving");
	Column(shut, 130, Wall);
	(void)still.Tick(shut, target, 1, s); // a clock that went backwards
	Check(!still.CurrentRoute().reachable, "a clock that jumps backwards forces a fresh search instead of freezing the route");
}

void Cost() {
	const Beacon::Settings s;
	auto world = Field({110, 110}, 1);
	const Position target{190, 110}; // eighty cells of open room: a realistic far lock
	constexpr int Routes = 100;
	double proved = 0;
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < Routes; ++i) proved += Beacon::FindRoute(world, target, s).length;
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / Routes;
	// Reported for comparison between runs, never asserted: a shared build
	// machine's wall clock is not a property of the route search.
	std::cout << "far target average route ms=" << ms << '\n';
	Check(proved > 0, "the timed searches really did prove a route");
}
}

void Probe() {
	const Beacon::Settings s;
	// 1. overlapping rooms: first room blocked, second open
	World overlap = Field({70, 110});
	overlap.rooms.front().cells.assign(static_cast<size_t>(Size) * Size, Wall);
	Fill(overlap, 0, 0, Size - 1, Size - 1, Wall);
	Fill(overlap, 60, 100, 100, 120, 0);
	Put(overlap, 90, 110, Wall); // a pillar in the first room
	overlap.rooms.push_back({85, 105, 10, 10, std::vector<uint16_t>(100, 0)}); // second room says open
	std::cout << "probe overlap: World::Cell=" << static_cast<int>(overlap.Cell(90, 110, Layer::Terrain))
		<< " routed=" << Beacon::FindRoute(overlap, At(95, 110), s).reachable << '\n';
	bool through = false;
	for (const auto cell : Beacon::FindRoute(overlap, At(95, 110), s).path) through |= Same(cell, At(90, 110));
	std::cout << "probe overlap through pillar=" << through << '\n';
	// 2. length vs octile on general slopes
	for (int dy = -30; dy <= 30; dy += 5) {
		auto open = Field({70, 110});
		const Position goal = At(110, 110 + dy);
		const auto r = Beacon::FindRoute(open, goal, s);
		std::cout << "probe slope dy=" << dy << " length=" << r.length << " octile=" << Octile(open.player, goal)
			<< " excess=" << r.length - Octile(open.player, goal)
			<< " headingDot=" << Dot(r.heading, Unit(Minus(goal, open.player))) << '\n';
	}
	// 3. a first update that arrives later than the lock delay
	auto world = Field({110, 110});
	Beacon::Tracker late;
	late.Lock(1000);
	std::cout << "probe late first tick ping=" << late.Tick(world, At(160, 110), 1000 + s.lockDelayMs + 500, s).ping << '\n';
	// 4. route length in a corridor with a bend
	const auto elbow = MakeElbow(1, -1);
	const auto bent = Beacon::FindRoute(elbow.world, elbow.target, s);
	std::cout << "probe elbow length=" << bent.length << " steps=" << bent.path.size()
		<< " octileLegs=" << Octile(elbow.player, elbow.corner) + Octile(elbow.corner, elbow.target) << '\n';
}
// The three words the beacon speaks when it locks: the player walks there, has
// to go around, or cannot get there at all -- and says nothing when the target
// is too far away for the search to have proved anything.
void ReachVerdict() {
	const Beacon::Settings s;

	auto open = Field({70, 110});
	const Position ahead{110, 110};
	const auto clear = Beacon::FindRoute(open, ahead, s);
	Check(clear.straight, "an open field between player and target is a straight walk");
	Check(Beacon::Reachability(clear, Length(Minus(ahead, open.player)), s) == Beacon::Reach::Straight,
		"a clear line is spoken as straight");

	// A wall across the line with a way round one end.
	auto detour = Field({70, 110});
	Fill(detour, 90, 90, 91, 130, Wall);
	const auto around = Beacon::FindRoute(detour, ahead, s);
	Check(!around.straight, "a wall across the line is not a straight walk");
	Check(around.reachable, "the target past the wall is still reachable");
	Check(Beacon::Reachability(around, Length(Minus(ahead, detour.player)), s) == Beacon::Reach::Around,
		"reachable but not in a line is spoken as around");

	// Sealed: no way in at all.
	auto sealed = Field({70, 110});
	Fill(sealed, 106, 106, 114, 114, Wall);
	Fill(sealed, 107, 107, 113, 113, 0);
	const auto none = Beacon::FindRoute(sealed, ahead, s);
	Check(!none.straight && !none.reachable, "a sealed target is neither straight nor reachable");
	Check(Beacon::Reachability(none, Length(Minus(ahead, sealed.player)), s) == Beacon::Reach::Unreachable,
		"a sealed target is spoken as no route");

	// Beyond the search box nothing was proved, so nothing is claimed.
	Check(Beacon::Reachability({}, s.searchRadius + 1, s) == Beacon::Reach::Unknown,
		"a target beyond the search box says nothing rather than no route");
	Check(Beacon::Reachability({}, std::numeric_limits<double>::quiet_NaN(), s) == Beacon::Reach::Unknown,
		"an unreadable distance says nothing");

	// A target standing on its own collision is still a straight walk up to it.
	auto post = Field({70, 110});
	Put(post, 110, 110, Wall);
	const auto toPost = Beacon::FindRoute(post, ahead, s);
	Check(!toPost.reachable, "the search never enters the target's own blocked cell");
	Check(Beacon::Reachability(toPost, Length(Minus(ahead, post.player)), s) == Beacon::Reach::Straight,
		"walking up to a chest in the open is straight, not unreachable");

	// The word every feature speaks comes from here, so they cannot disagree.
	const auto& english = Strings::For(Strings::Language::enUS);
	Check(Beacon::ReachWord(open, ahead, s, english) == english.beaconReachStraight, "a clear line says straight");
	Check(Beacon::ReachWord(detour, ahead, s, english) == english.beaconReachAround, "a wall in the way says around");
	Check(Beacon::ReachWord(sealed, ahead, s, english) == english.beaconReachNoRoute, "a sealed target says no route");
	Check(Beacon::ReachWord(open, {open.player.x + s.searchRadius + 10, open.player.y}, s, english) == nullptr,
		"beyond the search box there is no word to say");
}

int main() {
	Probe();
	PingCadence();
	OpenField();
	Corners();
	Doorway();
	SealedTarget();
	ReachVerdict();
	UnknownGround();
	BodyFootprint();
	WalkStability();
	TrackerLifecycle();
	TrackerCadence();
	TrackerReroute();
	Cost();
	Passed();
}
