#include "radar.h"
#include "radar_passage.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string_view>

using namespace D2RAccess;
using namespace Radar;
namespace {
constexpr int Size = 240;
// Native PLAYER_PATH walking mask and the collision bits it selects.
constexpr uint16_t PlayerPath = 0x1C09, Wall = 1, Blank = 0x20;
constexpr size_t Index(Direction direction) { return static_cast<size_t>(direction); }
constexpr size_t Down = Index(Direction::Down);

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
	Check(ahead.passage.observed == State::Unknown && ahead.passage.available
		&& ahead.passage.depth > ahead.passage.closeDepth && ahead.passage.depth < ahead.passage.openDepth,
		"that baseline really did land between the closing and the opening depth");
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
	Check(engine.Tick(world, 1300)[Down] == State::Unknown,
		"an opening confirmed inside the cue interval waits instead of sounding");
	Check(std::string_view(engine.Sensors()[Down].hold) == "interval"
		&& engine.Sensors()[Down].state == State::Open && engine.Sensors()[Down].deferred == State::Open,
		"the held opening is confirmed and remembered, not discarded");
	const uint64_t due = 1150 + Settings{}.cueIntervalMs;
	for (uint64_t now = 1350; now < due; now += PollMs)
		Check(engine.Tick(world, now)[Down] == State::Unknown, "the held opening stays silent for the rest of the interval");
	Check(engine.Tick(world, due)[Down] == State::Open,
		"the opening sounds later, once the interval since the previous cue has passed and it is still true");
	Barrier(world, 212, Wall);
	(void)engine.Tick(world, due + PollMs);
	(void)engine.Tick(world, due + 2 * PollMs);
	Check(engine.Tick(world, due + 3 * PollMs)[Down] == State::Blocked,
		"a wall confirmed well inside the interval since the last cue is never delayed by it");
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
	const uint64_t announced = run.now;
	Check(Silent(run.Step(-1, -1)) && Silent(run.Step(-1, -1)), "two steps back is still inside the dead band");
	const int lateralCells = static_cast<int>(1.0 / Diagonal); // one native cell of staircase wobble
	const int gapDifference = 2 * GapFromX - WallSum;                          // world x-y of the nearest gap cell
	const int difference = static_cast<int>(run.world.player.x - run.world.player.y);
	const int reachGap = StepsForward(difference + lateralCells, gapDifference);
	for (int step = 0; step < reachGap - 1; ++step)
		Check(Silent(run.Step(1, -1)), "the gap is still further sideways than the body may adjust");
	Check(Silent(run.Step(1, -1)), "the first sight of the route through the gap is only a candidate");
	Check(Silent(run.Step(1, -1)) && std::string_view(run.Ahead().hold) == "interval",
		"a route that really passes the wall is not withheld by the wall, only by the cue interval");
	Check(run.Ahead().state == State::Open, "that route is confirmed as an opening");
	State sounded = State::Unknown;
	uint64_t at = 0;
	while (run.now < announced + 4 * Settings{}.cueIntervalMs && sounded == State::Unknown) {
		const auto events = run.Step(1, -1);
		if (events[Down] != State::Unknown) { sounded = events[Down]; at = run.now; }
	}
	Check(sounded == State::Open, "the opening through the gap does sound");
	Check(at == announced + Settings{}.cueIntervalMs,
		"it sounds at the first poll a whole cue interval after the wall it replaces");
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
	(void)settle(0);
	Check(run.Ahead().state == State::Open && std::string_view(run.Ahead().hold) == "interval",
		"a route that now passes the door's place is not withheld by the memory of it");
	constexpr int Farther = 3;
	Check(settle(WallSum + Farther)[Down] == State::Blocked, "a second door further out is announced in its turn");
	Check(std::abs(run.Ahead().frontier.projection - first - Farther * Diagonal) < 1e-9,
		"the remembered wall moves out by exactly the three cells the new door added");
	Barrier(run.world, WallSum + Farther, 0);
	(void)settle(0);
	Check(run.Ahead().state == State::Open && std::string_view(run.Ahead().hold) == "interval",
		"passing the newest remembered wall releases the opening again");
	Check(run.Ahead().frontier.known, "an opening never erases the wall memory, it only outruns it");
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
	Thresholds();
	SilentBaseline();
	DeadBandBaseline();
	TravelConfirmation();
	StationaryConfirmation();
	IntervalHold();
	UnknownCoverage();
	Rebasing();
	RetreatKeepsTheWall();
	LateralEscapeReleasesTheWall();
	DoorwayPastTheWallSounds();
	FrontierFollowsTheLatestWall();
	RebaseForgetsTheWall();
	Passed();
}
