#include "radar_passage.h"
#include "camp_grid.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace D2RAccess;
using namespace Radar;
namespace {
constexpr int Size = 96;
constexpr uint16_t PlayerPath = 0x1C09, Barrier = 1, Blank = 0x20;
// The corridor floor. Five open rows leave the native five-cell body one cell
// of slack on each side, exactly as a real D2 corridor does.
constexpr int Floor = 38, FloorHeight = 5;
constexpr size_t Index(Direction direction) { return static_cast<size_t>(direction); }
// At rest the engine asks for its standing thresholds; every expectation below
// is computed against these two numbers, never against a literal.
const double Close = Engine::CloseDepth({}, 1), Open = Settings{}.openDepth;
// The scan window the engine uses: the full reach it measures beyond an opening.
const double Reach = std::max(Open, Settings{}.reachDepth);

World Field(Position player = {40, 40}, int pattern = 1) {
	World world;
	world.session = world.level = 1;
	world.player = player;
	world.bodyPattern = pattern;
	world.walkingMask = PlayerPath;
	world.rooms = {{0, 0, Size, Size, std::vector<uint16_t>(Size * Size)}};
	return world;
}
void Put(World& world, int x, int y, uint16_t flags) {
	world.rooms[0].cells[static_cast<size_t>(y) * Size + x] = flags;
}
// The tests' doorway width is the opening depth, the engine's fallback for it.
Passage Scan(const World& world, Position axis, double open = Open, double close = Close,
	State committed = State::Unknown, double wayThrough = Open) {
	auto grid = Navigation::Build(world, PassageRadius(std::max(Reach, open), world.bodyPattern));
	Navigation::LabelRooms(grid, world);
	return ScanPassage(grid, axis, {.openDepth = open, .closeDepth = close, .committed = committed,
		.maze = world.maze, .wayThrough = wayThrough, .exits = &world.exits});
}
void Corridor(World& world) {
	for (int x = 0; x < Size; ++x) {
		Put(world, x, Floor - 1, Barrier);
		Put(world, x, Floor + FloorHeight, Barrier);
	}
}

void CorridorStability() {
	// A corridor along world X. The screen cardinals are world diagonals, so
	// each of the four rays crosses the five-cell floor and stops one cell
	// short of the body-blocked row beside a wall. The point of the redesign
	// is that this verdict does not depend on where along the corridor the
	// player stands.
	// The route is straight: one native cell of tolerance, which on a world
	// diagonal is the single lattice step of its own staircase.
	const int wobble = static_cast<int>(std::floor(1.0 / Diagonal));
	// The five-cell floor leaves one body-clear row either side of the player's
	// row, so the farthest provable cell sits one row off the axis and that row
	// plus the wobble along it.
	const double depth = (1 + 1 + wobble) * Diagonal;
	for (int x = 26; x <= 70; ++x) {
		auto world = Field({static_cast<double>(x), Floor + 2});
		Corridor(world);
		for (size_t i = 0; i < DirectionCount; ++i) {
			const auto passage = Scan(world, Axes[i]);
			Check(passage.available && passage.observed == State::Blocked,
				"every screen cardinal meets a corridor wall and says so");
			Check(std::abs(passage.depth - depth) < 1e-9,
				"the proved depth along a corridor is the same wherever the player stands in it");
			Check(passage.openDepth == Open && passage.closeDepth == Close,
				"each observation echoes the thresholds it was judged against");
			Check(passage.depth <= passage.closeDepth, "a blocked verdict never exceeds its own closing threshold");
		}
		// Along the corridor there is a long route; across it there is one cell.
		const auto along = Scan(world, {1, 0}), across = Scan(world, {0, -1});
		Check(along.available && along.observed == State::Open && along.depth >= Open,
			"the world axis the corridor follows proves the required connected route");
		Check(Scan(world, {-1, 0}).observed == State::Open, "the corridor is open behind the player as well");
		Check(across.available && across.observed == State::Blocked && across.depth == 1,
			"across the corridor only the single body-clear cell beside the player is proved");
		Check(Scan(world, {0, 1}).depth == 1, "the opposite side of the corridor proves exactly as much");
	}
}

void OffsetDoorway() {
	// A doorway is reachable while the body's lateral adjustment stays within
	// one body span; beyond that the scan must not claim a route it cannot
	// prove. The world +X axis measures that offset in whole cells.
	const int wobble = static_cast<int>(1.0 / Diagonal);
	for (int offset = -wobble - 2; offset <= wobble + 2; ++offset) {
		auto world = Field();
		for (int y = 0; y < Size; ++y) Put(world, 44, y, Barrier);
		// Three rows of doorway: the five-cell cross needs a clear cell above
		// and below its centre to stand in the gap.
		for (int row = -1; row <= 1; ++row) Put(world, 44, 40 + offset + row, 0);
		const auto passage = Scan(world, {1, 0});
		Check(passage.available, "a fully mapped wall with a doorway is never short of coverage");
		Check((passage.observed == State::Open) == (std::abs(offset) <= wobble),
			"a doorway within one cell of the straight route is reachable and a farther one is not");
		if (std::abs(offset) == wobble + 1)
			Check(passage.observed == State::Blocked && passage.depth == 3,
				"the body still reaches the cell beside a doorway it cannot enter");
		if (std::abs(offset) > wobble + 1)
			Check(passage.observed == State::Blocked && passage.depth == 2,
				"a doorway further out leaves only the cells the wall does not touch");
	}
	auto solid = Field();
	for (int y = 0; y < Size; ++y) Put(solid, 44, y, Barrier);
	const auto closed = Scan(solid, {1, 0});
	Check(closed.available && closed.observed == State::Blocked && closed.depth == 2,
		"with no doorway at all the footprint stops one cell earlier, before the wall touches it");
}

void ObstacleBesideTheRay() {
	// The route is straight, so an obstacle that never touches it must not
	// close it. What does close it is the native body: the footprint reaches
	// one cell to each side of the ray, and the ray itself may wobble one cell.
	constexpr int Tile = 5; // native collision subtiles per map tile
	const int half = Tile / 2, bodyRadius = (static_cast<int>(BodySpan(1)) - 1) / 2;
	const int wobble = static_cast<int>(1.0 / Diagonal);
	const int blocking = half + bodyRadius - wobble;
	for (int side = 0; side <= blocking + 1; ++side) {
		auto world = Field();
		for (int row = -half; row <= half; ++row) Put(world, 42, 40 + side + row, Barrier);
		const auto passage = Scan(world, {1, 0});
		Check(passage.available, "a fully mapped obstacle beside the ray is never short of coverage");
		Check((passage.observed == State::Open) == (side > blocking),
			"an obstacle beside the ray closes the straight route only while the body cannot clear it");
	}
}

void NativeBodies() {
	// A one-cell hole in a wall: only the native point footprint fits.
	for (int pattern = 0; pattern <= 5; ++pattern) {
		const double close = Engine::CloseDepth({}, pattern), open = Settings{}.openDepth;
		auto world = Field({40, 40}, pattern);
		Check(Scan(world, {0, 1}, open, close).observed == State::Open,
			"every native body can advance across an open field");
		for (int x = 0; x < Size; ++x) Put(world, x, 44, Barrier);
		Put(world, 40, 44, 0);
		Check((Scan(world, {0, 1}, open, close).observed == State::Open) == (pattern == 0),
			"a one-cell opening in a wall admits the native point footprint only");
	}
}

void UnknownCoverage() {
	auto world = Field();
	for (int y = 0; y < Size; ++y) Put(world, 44, y, Blank);
	const auto passage = Scan(world, {1, 0});
	Check(!passage.available && passage.observed == State::Unknown,
		"unmapped cells across the route withdraw the verdict instead of closing it");
	Check(passage.depth == 2,
		"proof stops one cell before the footprint would have to test the unmapped column");
	Check(passage.openDepth == Open && passage.closeDepth == Close,
		"an unavailable observation still echoes the thresholds it was judged against");
	// Two resident rooms complete a route that either one alone cannot.
	world = Field();
	world.rooms.clear();
	world.rooms.push_back({30, 30, 15, 30, std::vector<uint16_t>(15 * 30)});
	world.rooms.push_back({45, 30, 15, 30, std::vector<uint16_t>(15 * 30)});
	Check(Scan(world, {1, 0}).observed == State::Open, "a complete body route crosses resident room seams");
	world.rooms.pop_back();
	const auto withdrawn = Scan(world, {1, 0});
	Check(!withdrawn.available && withdrawn.observed != State::Blocked,
		"unloading the second room withdraws proof instead of inventing closure");
	// Coverage only has to hold as far as the opening depth. An unmapped column
	// beyond it shortens the reach without withdrawing the proved opening.
	world = Field();
	constexpr int Far = 60;
	for (int y = 0; y < Size; ++y) Put(world, Far, y, Blank);
	const auto distant = Scan(world, {1, 0});
	Check(Far - 42 > Open, "the unmapped column really is beyond the opening depth");
	Check(distant.available && distant.observed == State::Open,
		"unmapped cells past the opening depth leave a proved opening standing");
	Check(distant.depth == Far - 42,
		"they do limit the reach to the last cell the footprint can be proved in");
}

void FullReach() {
	// The scan no longer stops at the opening depth: it reports the whole
	// proved route, so a later wall can be compared with an earlier one.
	auto world = Field();
	Corridor(world);
	constexpr int Length = 15;
	// The cross footprint stops one cell short of the closing wall.
	for (int y = 0; y < Size; ++y) Put(world, 40 + Length + 2, y, Barrier);
	const auto passage = Scan(world, {1, 0});
	Check(passage.available && passage.observed == State::Open, "a fifteen-cell corridor is an opening");
	Check(passage.depth == Length, "the reported depth is the whole corridor, not the opening threshold");
	Check(passage.depth > passage.openDepth, "the reach really does run past the threshold that decided the verdict");
	// Without the closing wall the reach is bounded by the scan window instead.
	auto endless = Field();
	Corridor(endless);
	const auto far = Scan(endless, {1, 0});
	Check(far.depth == PassageRadius(Reach, 1),
		"an open corridor is proved all the way to the edge of the scan window");
	Check(far.depth > passage.depth, "a longer corridor really does report a longer reach");
}

void CornerAndFirstStep() {
	// One wall cell diagonally in front of the player. The five-cell body cannot
	// enter any of the three cells the screen-down lane may start with, so the
	// lane is closed at its first step and reports exactly that.
	const size_t down = Index(Direction::Down);
	auto world = Field();
	Put(world, 41, 41, Barrier);
	const auto closed = Scan(world, Axes[down]);
	Check(closed.available && closed.observed == State::Blocked && closed.depth == 0,
		"a lane whose every first step is body-blocked closes at zero depth instead of withdrawing");
	auto point = Field({40, 40}, 0);
	Put(point, 41, 41, Barrier);
	Check(Scan(point, Axes[down], Open, Engine::CloseDepth({}, 0)).observed == State::Open,
		"the native point footprint walks past that same single wall cell");
	// Two walls touching at a corner, astride the lane. The body test on the
	// destination cell is the whole rule: the cross covers both wall cells and
	// cannot squeeze between them, while a point body may cut the corner.
	for (int pattern : {0, 1}) {
		auto corner = Field({39, 39}, pattern);
		Put(corner, 41, 40, Barrier);
		Put(corner, 40, 41, Barrier);
		const auto passage = Scan(corner, Axes[down], Open, Engine::CloseDepth({}, pattern));
		Check(passage.available && (passage.observed == State::Open) == (pattern == 0),
			"only a point body cuts the corner between two diagonally touching walls");
	}
	// Coverage, not geometry, is what withdraws a verdict: the same first step
	// made of unmapped cells is unknown instead of blocked.
	auto unmapped = Field();
	for (int x = 41; x <= 42; ++x) for (int y = 41; y <= 42; ++y) Put(unmapped, x, y, Blank);
	const auto withdrawn = Scan(unmapped, Axes[down]);
	Check(!withdrawn.available && withdrawn.observed == State::Unknown && withdrawn.depth == 0,
		"unmapped cells in the first step withdraw the verdict instead of closing the lane");
}

void DoorReading() {
	// A closed door is a wall the game marks with its own DOOR bit: the sound
	// says which, the distance and the hysteresis are the wall's.
	auto world = Field();
	for (int y = 0; y < Size; ++y) Put(world, 44, y, Collision::Door);
	const auto door = Scan(world, {1, 0});
	auto walled = Field();
	for (int y = 0; y < Size; ++y) Put(walled, 44, y, Barrier);
	const auto wall = Scan(walled, {1, 0});
	Check(door.available && door.observed == State::Door && door.door, "a closed door across the route is read as a door");
	Check(wall.available && wall.observed == State::Blocked && !wall.door, "a wall in the same place is read as a wall");
	Check(door.depth == wall.depth, "a door stops the body exactly where a wall would");
	auto far = Field();
	for (int y = 0; y < Size; ++y) Put(far, 40 + static_cast<int>(Open) + 5, y, Collision::Door);
	const auto ahead = Scan(far, {1, 0});
	Check(ahead.observed == State::Open && !ahead.door,
		"a door beyond the opening depth leaves the route open and is not yet what stops the player");
}

void NoDeadBand() {
	// One threshold each way. Between them the committed reading stands, and
	// with nothing committed the band reads as floor: a first observation is
	// always a verdict, never a silence a later wall could hide behind.
	auto world = Field();
	for (int y = 0; y < Size; ++y) Put(world, 40 + static_cast<int>(Close) + 3, y, Barrier);
	const auto fresh = Scan(world, {1, 0});
	Check(fresh.available && fresh.depth > Close && fresh.depth < Open, "the route really ends inside the band");
	Check(fresh.observed == State::Open, "with nothing committed the band reads as open floor");
	Check(Scan(world, {1, 0}, Open, Close, State::Open).observed == State::Open, "committed floor stands inside the band");
	Check(Scan(world, {1, 0}, Open, Close, State::Blocked).observed == State::Blocked, "a committed wall stands inside the band");
	Check(Scan(world, {1, 0}, Open, Close, State::Door).observed == State::Door,
		"a committed door stands inside the band as a door, never demoted to a wall");
	Check(Scan(world, {1, 0}, Open, Close, State::Exit).observed == State::Open,
		"a committed exit is read again inside the band: only obstacles are protected by it");
}

void ChamberExit() {
	// Two streamed rooms side by side. On a maze level a route that crosses
	// into the next chamber and clears its border by a doorway's width leads
	// somewhere; on any other level the partition means nothing.
	auto world = Field();
	world.rooms = {{20, 20, 25, 40, std::vector<uint16_t>(25 * 40)}, {45, 20, 25, 40, std::vector<uint16_t>(25 * 40)}};
	const auto floor = Scan(world, {1, 0});
	Check(floor.available && floor.observed == State::Open && floor.crossing == 5,
		"outside a maze the crossing is measured but the route is only floor");
	world.maze = true;
	const auto exit = Scan(world, {1, 0});
	Check(exit.observed == State::Exit && exit.depth >= exit.crossing + Open, "in a maze the route into the next chamber is an exit");
	Check(Scan(world, {-1, 0}).observed == State::Open, "the way deeper into the same chamber is only floor");
	Check(Scan(world, {1, 0}, Open, Close, State::Unknown, exit.depth - exit.crossing + 1).observed == State::Open,
		"a doorway wider than the route clears past the border is not proved");
	// A wall on the far side: the border is crossed but not cleared.
	auto shallow = world;
	for (int y = 0; y < 40; ++y) shallow.rooms[1].cells[static_cast<size_t>(y) * 25 + (54 - 45)] = Barrier;
	const auto crossed = Scan(shallow, {1, 0});
	Check(crossed.observed == State::Open && crossed.depth >= Open && crossed.crossing == 5,
		"crossing the border without a doorway's width beyond it is floor, not an exit");
}

void LevelExit() {
	// A level exit on the route: reached within a doorway of it, on the route's
	// own width. Exits are what a route on any level can lead to.
	auto world = Field();
	world.exits = {{54, 40}};
	Check(Scan(world, {1, 0}).observed == State::Exit, "a route arriving at a level exit leads somewhere");
	Check(Scan(world, {-1, 0}).observed == State::Open, "the same exit behind the player does not colour the route away from it");
	world.exits = {{54, 40 + static_cast<int>(Open)}};
	Check(Scan(world, {1, 0}).observed == State::Open, "an exit a doorway's width beside the route is not on it");
	world.exits = {{90, 40}};
	Check(Scan(world, {1, 0}).observed == State::Open, "an exit the route cannot be proved to reach is not announced");
	world.exits = {{54, 40}};
	for (int y = 0; y < Size; ++y) Put(world, 44, y, Barrier);
	Check(Scan(world, {1, 0}).observed == State::Blocked, "a wall before the exit is still a wall");
}

void RecordedCamp() {
	// Exact Ctrl+T capture: a camp post whose collision hits the side of the
	// native footprint while the centre cell beside it is empty.
	auto world = RecordedCampGrid();
	world.player = {4473, 4621};
	const auto grid = Navigation::Build(world, PassageRadius(Reach, world.bodyPattern));
	Check(grid.At(4466, 4628)->body == State::Blocked && world.Cell(4466, 4628) == State::Open,
		"the captured post blocks the native body while its own centre cell is clear");
	const auto left = ScanPassage(grid, Axes[Index(Direction::Left)], {.openDepth = Open, .closeDepth = Close});
	Check(left.available && left.observed == State::Open && left.depth >= Open,
		"the real capture proves the usable travel past the post without inventing the unseen map");
	Check(StructuralWorld(world, {}).Cell(4466, 4629) == State::Open,
		"the captured post has a verified detour, so structural geometry drops it");
	Check(world.Cell(4466, 4629) == State::Blocked,
		"dropping it structurally never edits the physical collision map");
	// Walking past it on that unchanging capture must stay silent.
	Engine engine;
	uint64_t now = 1000;
	bool silent = true;
	for (const double x : {4472.0, 4472.0, 4473.0, 4474.0, 4473.0, 4472.0, 4472.0, 4472.0}) {
		world.player = {x, 4622};
		const auto events = engine.Tick(world, now += PollMs);
		silent &= std::all_of(events.begin(), events.end(), [](State state) { return state == State::Unknown; });
	}
	Check(silent, "walking back and forth past the captured camp post produces no cue at all");
}
}
int main() {
	CorridorStability();
	OffsetDoorway();
	ObstacleBesideTheRay();
	NativeBodies();
	CornerAndFirstStep();
	UnknownCoverage();
	FullReach();
	DoorReading();
	NoDeadBand();
	ChamberExit();
	LevelExit();
	RecordedCamp();
	Passed();
}
