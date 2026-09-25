#include "radar.h"
#include "radar_passage.h"
#include "test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string_view>
#include <vector>

using namespace D2RAccess;
using namespace Radar;
namespace {
constexpr int Size = 240;
// Native PLAYER_PATH walking mask and the collision bits it selects.
constexpr uint16_t PlayerPath = 0x1C09, Wall = 1, Blank = 0x20;
constexpr size_t Index(Direction direction) { return static_cast<size_t>(direction); }
constexpr size_t Up = Index(Direction::Up), Down = Index(Direction::Down);

World Field() {
	World world;
	world.session = world.level = 1;
	world.player = {110, 100}; // world x+y = 210, the origin of every barrier below
	world.bodyPattern = 1; // native five-cell cross, the walking player footprint
	world.walkingMask = PlayerPath;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size)});
	return world;
}
void Put(World& world, int x, int y, uint16_t flags) {
	world.rooms[0].cells[static_cast<size_t>(y) * Size + x] = flags;
}
// A barrier along constant world x+y. It runs perpendicular to the screen
// up/down axes, so the down scan meets it head-on, and it crosses the whole
// structural window, so the clutter filter can never mistake it for furniture.
void Barrier(World& world, int sum, uint16_t flags) {
	for (int y = 0; y < Size; ++y) {
		const int x = sum - y;
		if (x >= 0 && x < Size) Put(world, x, y, flags);
	}
}
bool Silent(const Events& events) {
	return std::all_of(events.begin(), events.end(), [](State state) { return state == State::Unknown; });
}
int Sign(double value) { return value > 1e-9 ? 1 : value < -1e-9 ? -1 : 0; }

void AxisProjection() {
	// Native projection 0x334DB0: screen X = 80*(X-Y), screen Y = 40*(X+Y).
	// The four stick directions are the screen cardinals, so each one is a
	// world diagonal: the axes D2 walls, doorways and corridors follow.
	for (size_t i = 0; i < DirectionCount; ++i) {
		const double screenX = 80 * (Axes[i].x - Axes[i].y), screenY = 40 * (Axes[i].x + Axes[i].y);
		Check(Sign(screenX) == ScreenAxes[i].x && Sign(screenY) == ScreenAxes[i].y,
			"each world axis projects onto the screen components published for its direction");
		Check(std::abs(std::hypot(Axes[i].x, Axes[i].y) - 1) < 1e-12, "every stick axis is a unit world vector");
		Check(std::abs(std::abs(Axes[i].x) - std::numbers::sqrt2 / 2) < 1e-12
			&& std::abs(std::abs(Axes[i].y) - std::numbers::sqrt2 / 2) < 1e-12,
			"a screen cardinal is a world diagonal, never a world axis");
		Check(std::abs(ScreenAxes[i].x) + std::abs(ScreenAxes[i].y) == 1,
			"every direction is one screen cardinal, with no diagonal component");
	}
	const auto opposite = [](Direction a, Direction b) {
		return Axes[Index(a)].x == -Axes[Index(b)].x && Axes[Index(a)].y == -Axes[Index(b)].y
			&& ScreenAxes[Index(a)].x == -ScreenAxes[Index(b)].x && ScreenAxes[Index(a)].y == -ScreenAxes[Index(b)].y;
	};
	Check(opposite(Direction::Up, Direction::Down), "up and down are opposite on screen and in the world");
	Check(opposite(Direction::Left, Direction::Right), "left and right are opposite on screen and in the world");
	Check(ScreenAxes[Index(Direction::Up)].y < 0 && ScreenAxes[Index(Direction::Down)].y > 0
		&& ScreenAxes[Index(Direction::Left)].x < 0 && ScreenAxes[Index(Direction::Right)].x > 0,
		"each direction is named after the screen side it points to");
}

void Frames() {
	// Projection 0x334DB0 again: every ray of both frames sounds on the screen
	// side it points to and is named after the screen sector it falls in.
	for (const auto& frame : Rays) for (const auto& ray : frame) {
		const double screenX = 80 * (ray.world.x - ray.world.y), screenY = 40 * (ray.world.x + ray.world.y);
		Check(Sign(screenX) == ray.screen.x && Sign(screenY) == ray.screen.y, "every ray sounds on the screen side it points to");
		Check(std::abs(std::hypot(ray.world.x, ray.world.y) - 1) < 1e-12, "every ray is a unit world vector");
		Check(ScreenSector(ray.world) == ray.sector, "every ray is named after its own screen sector");
	}
	for (size_t i = 0; i < DirectionCount; ++i) {
		const auto& cardinal = RaysOf(Frame::Cardinal)[i];
		Check(cardinal.world.x == Axes[i].x && cardinal.world.y == Axes[i].y && cardinal.screen.x == ScreenAxes[i].x
			&& cardinal.screen.y == ScreenAxes[i].y, "the cardinal frame is the stick cardinals every other feature uses");
		const auto& diagonal = RaysOf(Frame::Diagonal)[i].world;
		Check((diagonal.x == 0) != (diagonal.y == 0), "a diagonal ray is a world axis, the way D2's walls run");
		Check(std::abs(diagonal.x * cardinal.world.x + diagonal.y * cardinal.world.y) - Diagonal < 1e-12,
			"and meets every cardinal ray at 45 degrees");
	}
}

void Thresholds() {
	const Settings settings;
	for (const int pattern : {0, 1}) {
		const double close = Engine::CloseDepth(settings, pattern);
		Check(close >= settings.blockedDepth, "a wall is never warned about later than the standing distance");
		Check(std::abs(settings.openDepth - close - BodySpan(pattern)) < 1e-12,
			"the hysteresis band is exactly one body span wide, so a body length of wobble cannot chatter");
	}
	Check(Engine::CloseDepth(settings, 0) > Engine::CloseDepth(settings, 1),
		"a one-cell body keeps its whole hysteresis band inside the configured opening depth");
	// Walking or running must not change any verdict, so the thresholds each
	// observation was judged against are the same at rest and at speed.
	auto resting = Field(), running = Field();
	Engine still, fast;
	uint64_t now = 1000;
	(void)still.Tick(resting, now);
	(void)fast.Tick(running, now);
	for (int step = 0; step < 5; ++step) {
		now += PollMs;
		running.player.x += 4; // eighty cells a second, far beyond a native run
		(void)still.Tick(resting, now);
		(void)fast.Tick(running, now);
	}
	Check(still.Speed() == 0 && fast.Speed() > 0, "the two runs really do differ in speed");
	for (size_t i = 0; i < DirectionCount; ++i) {
		const auto& slow = still.Sensors()[i].passage;
		const auto& quick = fast.Sensors()[i].passage;
		Check(slow.openDepth == quick.openDepth && slow.closeDepth == quick.closeDepth,
			"the same thresholds are used and echoed whether the player is standing or running");
		Check(slow.closeDepth == Engine::CloseDepth(settings, resting.bodyPattern)
			&& slow.openDepth == settings.openDepth,
			"the thresholds an observation echoes are the configured ones for that body");
	}
}

void SilentBaseline() {
	auto world = Field();
	Engine engine;
	Check(Silent(engine.Tick(world, 1000)), "the first known observation is a silent baseline");
	Check(engine.Revision() == 1, "taking up a position rebases once");
	for (uint64_t now = 1050; now <= 1400; now += PollMs)
		Check(Silent(engine.Tick(world, now)), "an unchanged map at rest never repeats itself");
	for (size_t i = 0; i < DirectionCount; ++i)
		Check(engine.Sensors()[i].state == State::Open && engine.Sensors()[i].passage.available,
			"the silent baseline still records the open field it observed in every direction");
	Check(engine.Revision() == 1, "standing still never rebases again");
}

void DeadBandBaseline() {
	// 2026-09-20 live run: up opened the session at depth 7.8, between the
	// closing and opening depths, so the wall the player then walked into was
	// swallowed as that direction's silent baseline and never sounded.
	auto world = Field();
	Barrier(world, 223, Wall); // eleven cells of route: the recorded 7.8 of depth
	Engine engine;
	uint64_t now = 1000;
	Check(Silent(engine.Tick(world, now)), "an observation inside the dead band is a silent baseline like any other");
	const auto& ahead = engine.Sensors()[Down];
	Check(ahead.passage.observed == State::Open && ahead.passage.available
		&& ahead.passage.depth > ahead.passage.closeDepth && ahead.passage.depth < ahead.passage.openDepth,
		"that baseline really did land between the closing and the opening depth, and read as floor");
	unsigned walls = 0, other = 0;
	const auto count = [&](const Events& events) {
		for (size_t i = 0; i < DirectionCount; ++i) {
			if (events[i] == State::Unknown) continue;
			if (i == Down && events[i] == State::Blocked) ++walls; else ++other;
		}
	};
	for (int step = 0; step < 5; ++step) {
		world.player.x += 1;
		world.player.y += 1;
		count(engine.Tick(world, now += PollMs));
	}
	world.player.x += 1; // the last cell the body fits in: no route left at all
	count(engine.Tick(world, now += PollMs));
	Check(ahead.state == State::Blocked && ahead.passage.depth == 0, "the player really did walk up to the wall");
	Check(walls == 1 && other == 0, "a wall reached from the dead band sounds exactly once, and nothing else does");
	// The other way round: a blocked baseline still lets the opening sound.
	world = Field();
	Barrier(world, 212, Wall);
	Engine opening;
	now = 1000;
	Check(Silent(opening.Tick(world, now)), "the blocked baseline is silent");
	Check(opening.Sensors()[Down].state == State::Blocked, "the baseline recorded the wall it saw");
	Barrier(world, 212, 0);
	unsigned openings = 0;
	other = 0;
	for (int poll = 0; poll < 10; ++poll) {
		const auto events = opening.Tick(world, now += PollMs);
		for (size_t i = 0; i < DirectionCount; ++i) {
			if (events[i] == State::Unknown) continue;
			if (i == Down && events[i] == State::Open) ++openings; else ++other;
		}
	}
	Check(openings == 1 && other == 0, "the first opening after a blocked baseline sounds exactly once");
}

void TravelConfirmation() {
	// The player walks one world cell per poll along +X, so he is never
	// stopped: the barrier ahead of the screen-down axis can only be confirmed
	// by the configured cell of travel. Walking along a world axis moves him
	// one cell closer to a x+y barrier per poll, which keeps the verdict
	// blocked throughout without ever putting his body inside the wall.
	auto world = Field();
	Engine engine;
	uint64_t now = 1000;
	Check(Silent(engine.Tick(world, now)), "the open baseline is silent");
	const auto walk = [&] { world.player.x += 1; now += PollMs; return engine.Tick(world, now); };
	Check(Silent(walk()), "walking through unchanged open ground stays silent");
	Barrier(world, 220, Wall);
	Check(walk()[Down] == State::Unknown, "the first observation of the wall is only a candidate");
	Check(engine.Sensors()[Down].candidate == State::Blocked, "the wall is held as a candidate while the player travels");
	Barrier(world, 220, 0);
	Check(walk()[Down] == State::Unknown, "a one-poll flicker back to open ground is never announced");
	Check(engine.Sensors()[Down].state == State::Open && engine.Sensors()[Down].candidate == State::Unknown,
		"the flicker withdraws the candidate and leaves the confirmed state untouched");
	Barrier(world, 220, Wall);
	const Position origin = world.player;
	Check(walk()[Down] == State::Unknown, "the returning wall starts its confirmation again from zero travel");
	Check(engine.Sensors()[Down].candidateOrigin.x == origin.x + 1
		&& engine.Sensors()[Down].candidateOrigin.y == origin.y,
		"confirmation is measured from where the candidate appeared");
	Check(walk()[Down] == State::Blocked, "one cell of travel with the wall still there confirms and sounds it");
	Check(engine.Sensors()[Down].state == State::Blocked && engine.Sensors()[Down].lastCue == State::Blocked,
		"the confirmed wall becomes the state the ear is tracking");
	Check(Silent(walk()) && Silent(walk()), "the confirmed wall is not repeated while the player keeps approaching it");
}

// The unexplored qualifier is part of the reading: an exit into a level never
// seen, floor towards the frontier. A change of the qualifier alone confirms
// and sounds like a change of state, and the marks travel with the events.
void UnexploredQualifier() {
	auto world = Field();
	world.exits = {{{120, 90}, 7, true}}; // on the screen-right axis, within reach
	world.frontier[Up] = true;
	Engine engine;
	Marks marks{};
	uint64_t now = 1000;
	// Under maxGapMs, so the engine keeps its evidence; over stoppedMs, so a stationary player confirms.
	const auto rest = [&] { now += 150; return engine.Tick(world, now, &marks); };
	Check(Silent(rest()), "the baseline is silent, marks included");
	constexpr size_t Right = Index(Direction::Right);
	Check(engine.Sensors()[Right].state == State::Exit && engine.Sensors()[Right].unexplored, "an exit into an unseen level reads unexplored");
	Check(engine.Sensors()[Up].state == State::Open && engine.Sensors()[Up].unexplored, "floor towards the frontier reads unexplored");
	Check(engine.Unexplored()[Right] && engine.Unexplored()[Up] && !engine.Unexplored()[Down], "the confirmed marks are exposed for the audio queue");
	world.exits[0].unexplored = false; // the level across got seen
	world.frontier[Up] = false; // and that quadrant is done
	Check(Silent(rest()), "a qualifier change is a candidate first");
	const auto events = rest();
	Check(events[Right] == State::Exit && !marks[Right], "the same exit sounds again, without the mark");
	Check(events[Up] == State::Open && !marks[Up], "the same floor sounds again, plain");
	Check(!engine.Sensors()[Right].unexplored && !engine.Unexplored()[Up], "the confirmed marks follow");
	Check(Silent(rest()), "and nothing repeats");
	// The chamber rule, on the qualifier alone: an exit into a room never stood in.
	World maze = Field();
	maze.rooms.push_back({Size, 0, Size, Size, {}});
	Passage passage;
	passage.available = true;
	passage.observed = State::Exit;
	passage.crossingRoom = 1;
	Check(Engine::Qualify(passage, maze, Right), "a chamber never stood in is unexplored");
	maze.rooms[1].visited = true;
	Check(!Engine::Qualify(passage, maze, Right), "a chamber stood in is not");
	passage.observed = State::Blocked;
	maze.frontier[Right] = true;
	Check(!Engine::Qualify(passage, maze, Right), "a wall never carries the qualifier");
}

void StationaryConfirmation() {
	auto world = Field();
	Barrier(world, 212, Wall); // two cells ahead: not one step of route is left
	Engine engine;
	Check(Silent(engine.Tick(world, 1000)), "the blocked baseline is silent");
	Check(engine.Sensors()[Down].state == State::Blocked && engine.Sensors()[Down].passage.depth == 0,
		"a wall one step beyond the footprint leaves no proved route at all");
	Barrier(world, 212, 0);
	Check(Silent(engine.Tick(world, 1050)), "a standing player's first opening observation is only a candidate");
	Check(Silent(engine.Tick(world, 1100)), "one poll is not enough to confirm a change without travel");
	Check(engine.Tick(world, 1150)[Down] == State::Open,
		"two polls of the same opening confirm it for a player who cannot travel");
	Check(engine.Sensors()[Down].passage.depth >= engine.Sensors()[Down].passage.openDepth,
		"the announced opening really proved the required connected route");
}

void IntervalHold() {
	auto world = Field();
	Engine engine;
	(void)engine.Tick(world, 1000); // open baseline
	Barrier(world, 212, Wall);
	(void)engine.Tick(world, 1050);
	(void)engine.Tick(world, 1100);
	Check(engine.Tick(world, 1150)[Down] == State::Blocked, "the confirmed wall sounds at once");
	Barrier(world, 212, 0);
	(void)engine.Tick(world, 1200);
	(void)engine.Tick(world, 1250);
	Check(engine.Tick(world, 1300)[Down] == State::Open,
		"an opening right after a wall is news, not a repeat: the wall cue never delays it");
	const uint64_t opened = 1300;
	Barrier(world, 212, Wall);
	(void)engine.Tick(world, 1350);
	(void)engine.Tick(world, 1400);
	Check(engine.Tick(world, 1450)[Down] == State::Blocked,
		"a wall confirmed well inside the interval since the opening is never delayed by it");
	Barrier(world, 212, 0);
	(void)engine.Tick(world, 1500);
	(void)engine.Tick(world, 1550);
	Check(engine.Tick(world, 1600)[Down] == State::Unknown,
		"a second opening inside the cue interval since the first one waits instead of sounding");
	Check(std::string_view(engine.Sensors()[Down].hold) == "interval"
		&& engine.Sensors()[Down].state == State::Open && engine.Sensors()[Down].deferred == State::Open,
		"the held opening is confirmed and remembered, not discarded");
	const uint64_t due = opened + Settings{}.cueIntervalMs;
	for (uint64_t now = 1650; now < due; now += PollMs)
		Check(engine.Tick(world, now)[Down] == State::Unknown, "the held opening stays silent for the rest of the interval");
	Check(engine.Tick(world, due)[Down] == State::Open,
		"the opening sounds later, once the interval since the previous opening has passed and it is still true");
}

void SameReadingIsNotRepeated() {
	// Floor that turns towards unexplored ground and straight back before its
	// cue could sound has not changed for the ear, so nothing is said.
	auto world = Field();
	Engine engine;
	uint64_t now = 1000;
	Barrier(world, 212, Wall);
	(void)engine.Tick(world, now); // wall baseline
	Barrier(world, 212, 0);
	const auto settle = [&] {
		(void)engine.Tick(world, now += PollMs);
		(void)engine.Tick(world, now += PollMs);
		return engine.Tick(world, now += PollMs)[Down];
	};
	Check(settle() == State::Open, "the floor is announced once the wall has gone");
	const uint64_t spoken = now;
	const auto& sensor = engine.Sensors()[Down];
	world.frontier[Down] = true;
	Check(settle() == State::Unknown && sensor.deferred == State::Open && sensor.deferredUnexplored
		&& std::string_view(sensor.hold) == "interval", "floor towards unexplored ground waits for the cue interval");
	world.frontier[Down] = false;
	Check(settle() == State::Unknown && sensor.deferred == State::Unknown && std::string_view(sensor.hold) == "unchanged",
		"plain floor again before that cue sounded is what the ear was last told, so it is dropped");
	while (now < spoken + 2 * Settings{}.cueIntervalMs)
		Check(engine.Tick(world, now += PollMs)[Down] == State::Unknown, "and nothing sounds once the interval is over");
}

// ---------------------------------------------------------------------------
// World-anchored wall memory. Walking into a wall is announced; backing out of
// it makes the route ahead long again, but the wall has not moved, so nothing
// opened and nothing may sound.
// ---------------------------------------------------------------------------
constexpr int WallSum = 250;  // world x+y of the barrier used by these tests
constexpr int StartSum = 210; // the player's world x+y at (110,100)
// The native cross footprint is stopped two cells short of a single-cell
// anti-diagonal wall, so that is the farthest the route can be proved to.
constexpr int Reachable = WallSum - 2;
const double Close = Engine::CloseDepth({}, 1), Open = Settings{}.openDepth;
// Along a screen axis one lattice step changes world x+y by two, so a whole
// number of cells of world x+y is that many multiples of Diagonal of depth.
int SumWhereBlocked() { return static_cast<int>(std::ceil(Reachable - Close / Diagonal)); }
int SumWhereOpen() { return static_cast<int>(std::floor(Reachable - Open / Diagonal)); }
int StepsForward(int from, int to) { return static_cast<int>(std::ceil((to - from) / 2.0)); }

struct Run {
	World world = Field();
	Engine engine;
	uint64_t now = 1000;
	Events Begin() { return engine.Tick(world, now); }
	Events Step(int dx, int dy) {
		world.player.x += dx;
		world.player.y += dy;
		now += PollMs;
		return engine.Tick(world, now);
	}
	Events Stay() { now += PollMs; return engine.Tick(world, now); }
	const Sensor& Ahead() const { return engine.Sensors()[Down]; }
	int Sum() const { return static_cast<int>(world.player.x + world.player.y); }
	// Walk straight at the wall until it is announced, and report that cue.
	void Approach() {
		const int silent = StepsForward(Sum(), SumWhereBlocked());
		for (int step = 0; step < silent; ++step)
			Check(Silent(Step(1, 1)), "approaching a wall is silent until the warning is confirmed");
		Check(Step(1, 1)[Down] == State::Blocked,
			"the wall is announced one confirming step after the route first falls inside the warning distance");
		Check(std::abs(Ahead().frontier.projection - Diagonal * Reachable) < 1e-9 && Ahead().frontier.known,
			"the announced wall is remembered at its own place in the world, not at a distance from the player");
	}
};

void RetreatKeepsTheWall() {
	Run run;
	Barrier(run.world, WallSum, Wall);
	Check(Silent(run.Begin()), "the distant wall is only a silent baseline");
	run.Approach();
	const Position announced = run.world.player;
	// Backing out along the same axis. The route grows past the opening depth
	// again, yet its far end is still the very same wall.
	const int toOpen = StepsForward(-run.Sum(), -SumWhereOpen()); // retreat is the other way
	for (int back = 1; back <= 10; ++back) {
		Check(Silent(run.Step(-1, -1)), "backing away from an announced wall never sounds like an opening");
		if (back < toOpen) continue;
		if (back == toOpen) {
			Check(run.Ahead().candidate == State::Open || run.Ahead().state == State::Open,
				"the longer route is seen as an opening again");
			continue;
		}
		Check(run.Ahead().state == State::Open && std::string_view(run.Ahead().hold) == "retreat"
			&& run.Ahead().frontier.known,
			"the opening is confirmed but withheld, because the route still ends at the remembered wall");
	}
	Check(run.Ahead().lastCue == State::Blocked, "the last thing the ear was told is still the wall");
	Check(announced.x - run.world.player.x == 10, "the player really has backed ten cells out");
	// A proximity warning, not a one-off: walking back into it warns again.
	const int forward = StepsForward(run.Sum(), SumWhereBlocked());
	for (int step = 0; step < forward; ++step)
		Check(Silent(run.Step(1, 1)), "walking back towards the known wall is silent until it is close again");
	Check(run.Step(1, 1)[Down] == State::Blocked, "approaching the same wall a second time warns again");
}

void LateralEscapeReleasesTheWall() {
	Run run;
	Barrier(run.world, WallSum, Wall);
	Check(Silent(run.Begin()), "the distant wall is only a silent baseline");
	run.Approach();
	// Sidestep along the wall past its neighbourhood. The wall is still in
	// front the whole way, so nothing is announced while moving sideways.
	const int sidesteps = static_cast<int>(std::floor(Settings{}.boundaryRange / std::numbers::sqrt2)) + 1;
	for (int step = 0; step < sidesteps; ++step)
		Check(Silent(run.Step(1, -1)), "walking along a wall changes nothing about the route into it");
	const int toOpen = StepsForward(-run.Sum(), -SumWhereOpen());
	for (int back = 1; back < toOpen; ++back) Check(Silent(run.Step(-1, -1)), "the route is not open yet");
	Check(Silent(run.Step(-1, -1)), "the first observation of the longer route is only a candidate");
	Check(run.Step(-1, -1)[Down] == State::Open,
		"a wall left far behind sideways no longer gates the opening ahead of the player");
}

void DoorwayPastTheWallSounds() {
	Run run;
	Barrier(run.world, WallSum, Wall);
	// A gap in the wall, out of reach of the approach lane: the scan admits at
	// most one body span of lateral adjustment, which on a screen axis is four
	// whole steps of world x-y.
	constexpr int GapFromX = 135, GapToX = 145;
	for (int x = GapFromX; x <= GapToX; ++x) Put(run.world, x, WallSum - x, 0);
	Check(Silent(run.Begin()), "the distant wall is only a silent baseline");
	run.Approach();
	Check(Silent(run.Step(-1, -1)) && Silent(run.Step(-1, -1)), "two steps back is still inside the dead band");
	const int lateralCells = static_cast<int>(1.0 / Diagonal); // one native cell of staircase wobble
	const int gapDifference = 2 * GapFromX - WallSum;                          // world x-y of the nearest gap cell
	const int difference = static_cast<int>(run.world.player.x - run.world.player.y);
	const int reachGap = StepsForward(difference + lateralCells, gapDifference);
	for (int step = 0; step < reachGap - 1; ++step)
		Check(Silent(run.Step(1, -1)), "the gap is still further sideways than the body may adjust");
	Check(Silent(run.Step(1, -1)), "the first sight of the route through the gap is only a candidate");
	Check(run.Step(1, -1)[Down] == State::Open,
		"a route that really passes the wall sounds once confirmed: neither the wall nor its cue holds it back");
}

void FrontierFollowsTheLatestWall() {
	// Standing still while a door closes, opens, and is replaced by a second
	// door exactly three cells further away.
	Run run;
	const Position stand{127, 117};
	run.world.player = stand;
	Check(Silent(run.Begin()), "the open baseline is silent");
	const auto settle = [&](int sum) {
		if (sum) Barrier(run.world, sum, Wall);
		(void)run.Stay();
		(void)run.Stay();
		return run.Stay();
	};
	Check(settle(WallSum)[Down] == State::Blocked, "the closing door is announced");
	const double first = run.Ahead().frontier.projection;
	Check(std::abs(first - Diagonal * Reachable) < 1e-9, "the door is remembered at its own place in the world");
	Barrier(run.world, WallSum, 0);
	Check(settle(0)[Down] == State::Open, "a route that now passes the door's place is not withheld by the memory of it");
	constexpr int Farther = 3;
	Check(settle(WallSum + Farther)[Down] == State::Blocked, "a second door further out is announced in its turn");
	Check(std::abs(run.Ahead().frontier.projection - first - Farther * Diagonal) < 1e-9,
		"the remembered wall moves out by exactly the three cells the new door added");
	Barrier(run.world, WallSum + Farther, 0);
	(void)settle(0);
	Check(run.Ahead().state == State::Open && std::string_view(run.Ahead().hold) == "interval",
		"passing the newest remembered wall releases the opening again, held only by the interval since the first");
	Check(run.Ahead().frontier.known, "an opening never erases the wall memory, it only outruns it");
}

// D2's own walls run along the world axes, at 45 degrees to every screen ray:
// here one along world x, with a gap one map tile wide a little further on.
void AlongAnAxisWall() {
	constexpr int WallY = 125, GapFrom = 129, GapWidth = 5;
	const auto approach = [](Run& run) {
		run.world.player = {100, 100};
		for (int x = 0; x < Size; ++x) if (x < GapFrom || x >= GapFrom + GapWidth) Put(run.world, x, WallY, Wall);
		Check(Silent(run.Begin()), "the distant wall is only a silent baseline");
		State heard = State::Unknown;
		for (int step = 0; step < 30 && heard == State::Unknown; ++step) heard = run.Step(1, 1)[Down];
		Check(heard == State::Blocked, "walking at the wall announces it");
	};
	{
		// Walking along it close by, at once and faster than running (a cell
		// per poll), finds the gap: floor through it, the wall again after it.
		Run run;
		approach(run);
		std::vector<State> heard;
		for (int step = 0; step < 16; ++step)
			if (const auto events = run.Step(1, 0); events[Down] != State::Unknown) heard.push_back(events[Down]);
		Check(heard == std::vector<State>{State::Open, State::Blocked},
			"a gap reached right after its wall was announced sounds, and the wall after it too");
	}
	{
		// Backing off first and then walking along it: the route ahead meets the
		// same wall further along, which is no opening.
		Run run;
		approach(run);
		for (int back = 0; back < 8; ++back)
			Check(run.Step(-1, -1)[Down] == State::Unknown, "backing away from the wall never sounds like an opening ahead");
		for (int step = 0; step < 6; ++step)
			Check(run.Step(1, 0)[Down] == State::Unknown, "walking along the same wall further off says nothing");
	}
}

// A corridor along world x, the way D2 lays them out: seven cells between its
// walls, a one-tile doorway in one of them, and a wall across its far end.
void CorridorAlongAWorldAxis() {
	constexpr int North = 96, South = 104, DoorFrom = 130, DoorTo = 134, End = 150;
	const auto corridor = [] {
		auto world = Field();
		for (int x = 0; x < Size; ++x) { Put(world, x, North, Wall); Put(world, x, South, Wall); }
		return world;
	};
	{
		Engine engine;
		auto world = corridor();
		(void)engine.Tick(world, 1000);
		for (const auto& sensor : engine.Sensors())
			Check(sensor.state == State::Blocked, "on the stick cardinals every ray meets a corridor wall at 45 degrees: all four read wall");
	}
	Engine engine({.frame = Frame::Diagonal});
	auto world = corridor();
	for (int x = DoorFrom; x <= DoorTo; ++x) Put(world, x, North, 0);
	for (int y = North; y <= South; ++y) Put(world, End, y, Wall);
	uint64_t now = 1000;
	Check(Silent(engine.Tick(world, now)), "the baseline is silent");
	const auto& rays = RaysOf(Frame::Diagonal);
	for (size_t i = 0; i < DirectionCount; ++i)
		Check(engine.Sensors()[i].state == (rays[i].world.y == 0 ? State::Open : State::Blocked),
			"on the diagonals the two rays along the corridor read floor and the two across it read wall");
	std::array<std::vector<State>, DirectionCount> heard;
	while (world.player.x < End - Engine::CloseDepth({}, world.bodyPattern)) {
		world.player.x += 1; // one cell per poll, faster than running
		const auto events = engine.Tick(world, now += PollMs);
		for (size_t i = 0; i < DirectionCount; ++i) if (events[i] != State::Unknown) heard[i].push_back(events[i]);
	}
	for (size_t i = 0; i < DirectionCount; ++i) {
		const auto& axis = rays[i].world;
		if (axis.y < 0) Check(heard[i] == std::vector<State>{State::Open, State::Blocked},
			"walking along it, the doorway in the side wall sounds as it passes, and the wall after it");
		if (axis.y > 0 || axis.x < 0) Check(heard[i].empty(), "the plain side wall and the way back say nothing new");
		if (axis.x > 0) Check(heard[i] == std::vector<State>{State::Blocked}, "the wall across the far end is announced ahead");
	}
}

// A small stone on open ground, square across the screen-down route.
void RockInTheWay() {
	for (const bool slides : {true, false}) {
		Run run;
		run.world.slides = slides;
		for (int y = 110; y <= 112; ++y) for (int x = 120; x <= 122; ++x) Put(run.world, x, y, Wall);
		Check(Silent(run.Begin()), "a distant stone is only a silent baseline");
		// Nine diagonal steps would put the cross footprint against it; eight stay clear.
		Events heard{};
		for (int step = 0; step < 8; ++step) {
			const auto events = run.Step(1, 1);
			if (events[Down] != State::Unknown) heard[Down] = events[Down];
		}
		if (slides) Check(heard[Down] == State::Unknown, "while the game steers round small obstacles, a lone stone is no wall");
		else Check(heard[Down] == State::Blocked, "when a direct move stops at obstacles, the stone ahead is announced as a wall");
	}
}

void DoorAhead() {
	Run run;
	Barrier(run.world, WallSum, Collision::Door);
	Check(Silent(run.Begin()), "a distant door is only a silent baseline");
	const int silent = StepsForward(run.Sum(), SumWhereBlocked());
	for (int step = 0; step < silent; ++step)
		Check(Silent(run.Step(1, 1)), "approaching a door is silent until the warning is confirmed");
	Check(run.Step(1, 1)[Down] == State::Door, "a closed door is announced as a door, at a wall's distance");
	Check(run.Ahead().frontier.known, "a door is remembered like a wall");
	for (int back = 1; back <= 3; ++back)
		Check(Silent(run.Step(-1, -1)) && run.Ahead().state == State::Door, "backing off a door is silent, and it stays a door");
}

void WayBackTakenAsRead() {
	// Two chambers of a maze split along world x. Walking screen-down crosses
	// from the first into the second; the doorway just crossed then reads as
	// an exit behind the player, and is taken as read instead of sounded.
	auto world = Field();
	world.maze = true;
	world.rooms = {{0, 0, 120, Size, std::vector<uint16_t>(120 * Size)}, {120, 0, 120, Size, std::vector<uint16_t>(120 * Size)}};
	Engine engine;
	uint64_t now = 1000;
	Check(Silent(engine.Tick(world, now)), "the baseline is silent");
	Check(engine.Sensors()[Down].state == State::Exit, "ahead, the route into the next chamber is an exit from the start");
	Check(engine.Sensors()[Up].state == State::Open, "behind, the first chamber is only floor");
	Events heard{};
	while (world.player.x < 120 + 6) {
		world.player.x += 1;
		world.player.y += 1;
		const auto events = engine.Tick(world, now += PollMs);
		for (size_t i = 0; i < DirectionCount; ++i) if (events[i] != State::Unknown) heard[i] = events[i];
	}
	Check(heard[Up] == State::Unknown, "the doorway just crossed never sounds behind the player");
	Check(engine.Sensors()[Up].state == State::Exit && engine.Sensors()[Up].lastCue == State::Exit
		&& std::string_view(engine.Sensors()[Up].hold) == "way-back", "it is confirmed and taken as read");
	Check(heard[Down] == State::Open, "ahead, the second chamber's floor is announced once the exit is behind");
	Check(std::abs(engine.CurrentWayThrough() - Settings{}.openDepth) < 1e-9,
		"a border open along its whole length is clamped to the opening depth as the doorway width");
}

void WayBackLapses() {
	// Three maze chambers side by side along world x, the middle one narrow.
	// Walking screen-down from it into the last, the way back reads as an exit
	// before and after the border, so nothing confirms that would spend the mark.
	auto world = Field();
	world.maze = true;
	world.player = {112, 100};
	world.rooms = {{0, 0, 110, Size, std::vector<uint16_t>(110 * Size)}, {110, 0, 10, Size, std::vector<uint16_t>(10 * Size)},
		{120, 0, 120, Size, std::vector<uint16_t>(120 * Size)}};
	Engine engine;
	uint64_t now = 1000;
	Check(Silent(engine.Tick(world, now)), "the baseline is silent");
	const auto& behind = engine.Sensors()[Up];
	Check(behind.state == State::Exit, "behind, the route into the first chamber is an exit");
	const auto step = [&] { world.player.x += 1; world.player.y += 1; return engine.Tick(world, now += PollMs); };
	while (world.player.x < 120) Check(step()[Up] == State::Unknown, "walking on towards the border changes nothing behind");
	const Position crossed = world.player;
	Check(behind.wayBack && behind.state == State::Exit, "crossing marks the way back, which reads as an exit as before");
	const auto travelled = [&] { return std::hypot(world.player.x - crossed.x, world.player.y - crossed.y); };
	while (travelled() + std::numbers::sqrt2 <= engine.CurrentWayThrough()) {
		(void)step();
		Check(behind.wayBack, "within a doorway's width of the border the mark stands");
	}
	(void)step();
	Check(!behind.wayBack, "one step further the mark has lapsed: it is not a memory of the doorway");
	world.rooms[1].visited = true; // the way back now leads into explored ground: a real change
	State heard = State::Unknown;
	for (int poll = 0; poll < 8 && heard == State::Unknown; ++poll) heard = engine.Tick(world, now += PollMs)[Up];
	Check(heard == State::Exit, "a later change of the way back is announced, not taken as read");
}

void RebaseForgetsTheWall() {
	Run run;
	Barrier(run.world, WallSum, Wall);
	Check(Silent(run.Begin()), "the distant wall is only a silent baseline");
	run.Approach();
	Check(run.Ahead().frontier.known, "the wall is remembered while the level lasts");
	++run.world.level;
	(void)run.Stay();
	Check(!run.engine.Sensors()[Down].frontier.known, "a new level forgets every wall the old one taught");
	for (size_t i = 0; i < DirectionCount; ++i)
		Check(!run.engine.Sensors()[i].frontier.known && run.engine.Sensors()[i].lastCue == State::Unknown,
			"the rebase clears the memory of every direction, not just the one that was announced");
}

void UnknownCoverage() {
	auto world = Field();
	Engine engine;
	(void)engine.Tick(world, 1000); // open baseline
	Barrier(world, 212, Blank);     // the map stops being resident across the route
	uint64_t now = 1000;
	for (int poll = 0; poll < 6; ++poll)
		Check(Silent(engine.Tick(world, now += PollMs)), "losing coverage in a direction never invents a wall");
	const auto& sensor = engine.Sensors()[Down];
	Check(!sensor.passage.available && sensor.passage.observed == State::Unknown,
		"an unmapped cell across the route withdraws the verdict instead of closing it");
	Check(sensor.state == State::Open && sensor.candidate == State::Unknown,
		"the confirmed state survives the coverage loss and no confirmation runs while it lasts");
	Barrier(world, 212, Wall);
	(void)engine.Tick(world, now += PollMs);
	(void)engine.Tick(world, now += PollMs);
	Check(engine.Tick(world, now += PollMs)[Down] == State::Blocked,
		"restored coverage resumes confirmation from the state kept during the gap");
}

void Rebasing() {
	const Settings settings;
	{
		auto world = Field();
		Barrier(world, 212, Wall);
		Engine engine;
		(void)engine.Tick(world, 1000);
		const uint64_t revision = engine.Revision();
		Barrier(world, 212, 0);
		++world.level;
		Check(Silent(engine.Tick(world, 1050)), "entering a new level discards live evidence and rebases silently");
		Check(engine.Revision() == revision + 1, "the rebase bumps the revision so pending audio is cancelled");
		for (uint64_t now = 1100; now <= 1400; now += PollMs)
			Check(Silent(engine.Tick(world, now)), "the new level's baseline is never announced");
		Check(engine.Sensors()[Down].state == State::Open, "the rebased baseline records the new level's geometry");
	}
	{
		auto world = Field();
		Barrier(world, 212, Wall);
		Engine engine;
		(void)engine.Tick(world, 1000);
		const uint64_t revision = engine.Revision();
		Barrier(world, 212, 0);
		++world.session;
		Check(Silent(engine.Tick(world, 1050)) && engine.Revision() == revision + 1,
			"a new session rebases exactly like a new level");
	}
	{
		auto world = Field();
		Barrier(world, 212, Wall);
		Engine engine;
		(void)engine.Tick(world, 1000);
		const uint64_t revision = engine.Revision();
		Barrier(world, 212, 0);
		const uint64_t resumed = 1000 + settings.maxGapMs + 1;
		Check(Silent(engine.Tick(world, resumed)), "a poll gap longer than the configured maximum discards live evidence");
		Check(engine.Revision() == revision + 1, "the discarded evidence cancels pending audio");
		Check(Silent(engine.Tick(world, resumed + PollMs)) && Silent(engine.Tick(world, resumed + 2 * PollMs)),
			"the observation that arrived after the gap is only a baseline");
	}
	{
		auto world = Field();
		Barrier(world, 212, Wall);
		Engine engine;
		(void)engine.Tick(world, 1000);
		const uint64_t revision = engine.Revision();
		Barrier(world, 212, 0);
		const uint64_t resumed = 1000 + settings.maxGapMs;
		Check(Silent(engine.Tick(world, resumed)) && engine.Revision() == revision,
			"the longest accepted gap keeps the evidence and only starts a candidate");
		Check(Silent(engine.Tick(world, resumed + PollMs)), "the surviving candidate still needs its second poll");
		Check(engine.Tick(world, resumed + 2 * PollMs)[Down] == State::Open,
			"the candidate that survived the accepted gap confirms normally");
	}
	{
		auto world = Field();
		Engine engine;
		(void)engine.Tick(world, 1000);
		const uint64_t revision = engine.Revision();
		world.player.x += settings.teleportDistance;
		Barrier(world, static_cast<int>(210 + settings.teleportDistance + 2), Wall);
		Check(Silent(engine.Tick(world, 1050)),
			"a jump of the teleport distance between two polls rebases instead of describing the new surroundings");
		Check(engine.Revision() == revision + 1, "the teleport cancels pending audio for the old position");
	}
	{
		auto world = Field();
		Engine engine;
		(void)engine.Tick(world, 1000);
		const uint64_t revision = engine.Revision();
		world.player.x += settings.teleportDistance - 1;
		(void)engine.Tick(world, 1050);
		Check(engine.Revision() == revision, "a long but continuous step keeps the current observations");
	}
}
}
int main() {
	AxisProjection();
	Frames();
	Thresholds();
	SilentBaseline();
	DeadBandBaseline();
	TravelConfirmation();
	UnexploredQualifier();
	StationaryConfirmation();
	IntervalHold();
	SameReadingIsNotRepeated();
	UnknownCoverage();
	Rebasing();
	RetreatKeepsTheWall();
	LateralEscapeReleasesTheWall();
	DoorwayPastTheWallSounds();
	FrontierFollowsTheLatestWall();
	AlongAnAxisWall();
	RockInTheWay();
	CorridorAlongAWorldAxis();
	DoorAhead();
	WayBackTakenAsRead();
	WayBackLapses();
	RebaseForgetsTheWall();
	Passed();
}
