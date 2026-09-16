#include "radar.h"
#include "test_support.h"
#include <algorithm>
#include <cmath>
#include <string_view>

using namespace D2RAccess::Radar;
namespace {
World Field() {
	World world;
	world.session = world.level = 1;
	world.player = {100, 100}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
	world.rooms.push_back({0, 0, 240, 240, std::vector<uint16_t>(240 * 240)});
	return world;
}
void Wall(World& world, int sum, uint16_t flags = 1) {
	for (int y = 0; y < 240; ++y)
		if (sum-y >= 0 && sum-y < 240) world.rooms[0].cells[y*240+sum-y] = flags;
}
bool Silent(const Events& events) {
	return std::all_of(events.begin(), events.end(), [](auto state) { return state == State::Unknown; });
}
void Speed() {
	// Live 07:09 session: the same hit/10.5-subtile clearance flipped closed
	// to open in 125 ms when measured speed fell. No new passage existed.
	auto world = Field(); Wall(world, 216);
	Engine engine; (void)engine.Tick(world, 1000);
	AudioLoad busy; busy.busy = true; busy.pending.fill(State::Blocked);
	unsigned events = 0;
	for (uint64_t tick = 1; tick <= 40; ++tick) {
		world.subtileFraction = tick % 8 < 4 ? Position{0.9, 0.9} : Position{0.1, 0.1};
		events += !Silent(engine.Tick(world, 1000 + tick*50, tick % 3 ? busy : AudioLoad{}));
	}
	Check(events == 0, "speed, fractional motion and audio load cannot invent geometry changes in an unchanged cell");
}
void Encounter() {
	auto world = Field(); Wall(world, 260);
	Engine engine; (void)engine.Tick(world, 1000);
	uint64_t now = 1000;
	unsigned open = 0, closed = 0;
	const auto tick = [&] {
		const auto event = engine.Tick(world, now += 50)[3];
		open += event == State::Open; closed += event == State::Blocked;
	};
	for (int step = 0; step < 28; ++step) { ++world.player.x; ++world.player.y; tick(); }
	for (int step = 0; step < 12; ++step) tick();
	Check(closed == 1 && open == 0, "approaching then stopping at one wall produces one encounter, without a second proximity warning or false opening");
	for (int step = 0; step < 28; ++step) { --world.player.x; --world.player.y; tick(); }
	Check(closed == 1 && open == 0, "leaving the local wall range retains its reference without inventing an exit");
}
void RangeEdge() {
	BoundaryTracker tracker;
	Probe probe; probe.end = State::Blocked; probe.clearDepth = 19; probe.hit = {120, 100};
	uint64_t now = 1000;
	const auto tick = [&] { return tracker.Observe(probe, {1, 0}, now += 50, PollMs, 20); };
	(void)tick();
	unsigned events = 0;
	for (int step = 0; step < 24; ++step) {
		probe.clearDepth = (step / 3) % 2 ? 20.5 : 19;
		events += tick() != State::Unknown;
	}
	Check(events == 0, "one diagonal collision step across the scan limit cannot chatter open/closed");
	probe.clearDepth = 23; (void)tick();
	Check(tick() == State::Open, "moving clearly beyond the range still announces the lost wall");
	probe.clearDepth = 19; (void)tick();
	Check(tick() == State::Blocked, "a real return into range is not hidden by a cooldown");
}
void ContinuousWall() {
	// A world-axis wall is oblique to the fixed screen rays. Three cells of
	// travel between polls move the hit >1.5 along the ray without a recess.
	for (int stride : {1, 3}) {
		auto world = Field(); world.player.y = 90;
		for (int x = 0; x < 240; ++x) world.rooms[0].cells[80*240+x] = 1;
		Engine engine; (void)engine.Tick(world, 1000);
		unsigned events = 0;
		for (uint64_t step = 1; step <= 20; ++step) {
			world.player.x += stride;
			events += !Silent(engine.Tick(world, 1000 + step*stride*50));
		}
		Check(events == 0, "following a continuous oblique wall stays silent at normal and delayed polling rates");
	}
}
void UnusableNiches() {
	auto world = Field(); Wall(world, 206); Wall(world, 212);
	Engine engine; (void)engine.Tick(world, 1000);
	uint64_t now = 1000;
	unsigned events = 0;
	for (int pass = 0; pass < 3; ++pass) {
		// The recessed boundary is still too shallow for a useful passage.
		// Rejecting its opening must also suppress the return to the old wall.
		Wall(world, 206, 0);
		for (int step = 0; step < 4; ++step) events += !Silent(engine.Tick(world, now += 50));
		Wall(world, 206);
		for (int step = 0; step < 4; ++step) events += !Silent(engine.Tick(world, now += 50));
	}
	Check(events == 0, "unusable recesses do not spam unpaired closing cues when the original boundary returns");
	Wall(world, 206, 0); Wall(world, 212, 0);
	(void)engine.Tick(world, now += 50);
	Check(engine.Tick(world, now += 50)[3] == State::Open, "filtering niches preserves the next real opening");
	Wall(world, 206); (void)engine.Tick(world, now += 50);
	Check(engine.Tick(world, now += 50)[3] == State::Blocked, "a previously announced real opening still announces its closure");
}
void RecessAtSpeed() {
	for (int stride : {1, 3, 4}) {
		auto world = Field();
		for (int x = 0; x < 240; ++x) {
			world.rooms[0].cells[85*240+x] = 1;
			if (x < 118 || x > 135) world.rooms[0].cells[90*240+x] = 1;
		}
		Engine engine; (void)engine.Tick(world, 1000);
		uint64_t now = 1000;
		std::array<unsigned, DirectionCount> open{}, closed{};
		for (int sign : {1, -1}) for (int step = 0; step < 60 / stride; ++step) {
			world.player.x += stride * sign;
			const auto events = engine.Tick(world, now += stride*50);
			for (size_t i = 0; i < DirectionCount; ++i) {
				open[i] += events[i] == State::Open; closed[i] += events[i] == State::Blocked;
			}
		}
		for (size_t direction : {size_t{0}, size_t{2}}) {
			if (open[direction] != 0 || closed[direction] != 0)
				std::cerr << "recess stride=" << stride << " direction=" << direction << " open=" << open[direction] << " closed=" << closed[direction] << '\n';
			Check(open[direction] == 0 && closed[direction] == 0,
				"far wall recesses stay silent on both rays at every tested speed/poll interval");
		}
		Check(open[1] == 0 && closed[1] == 0 && open[3] == 0 && closed[3] == 0,
			"recesses do not invent changes on the opposite clear side");
	}
}
void PendingCoverageLoss() {
	auto world = Field(); Wall(world, 206); Wall(world, 226);
	Engine engine; (void)engine.Tick(world, 1000);
	Wall(world, 206, 0); world.rooms[0].cells[101*240+101] = 0x1100;
	(void)engine.Tick(world, 1050); (void)engine.Tick(world, 1100);
	Check(engine.Sensors()[3].passageState.Pending() == State::Open, "occupied recess waits for physical clearance");
	Wall(world, 212, 0x20); (void)engine.Tick(world, 1150);
	Check(engine.Sensors()[3].passageState.Pending() == State::Unknown && engine.Sensors()[3].cueState == State::Unknown,
		"missing local geometry clears old cues when the short connected route is no longer covered");
	Wall(world, 212, 0); world.rooms[0].cells[101*240+101] = 0;
	for (uint64_t now = 1200; now <= 1600; now += 50)
		Check(Silent(engine.Tick(world, now)), "restored local coverage cannot replay an opening from before data loss");
	Wall(world, 206); (void)engine.Tick(world, 1650);
	Check(engine.Tick(world, 1700)[3] == State::Blocked, "fresh geometry after coverage recovery still announces a real narrowing");
}
void PendingWallDrift() {
	BoundaryTracker tracker;
	Probe probe; probe.end = State::Blocked; probe.clearDepth = 10; probe.hit = {120, 100};
	uint64_t now = 1000;
	const auto tick = [&] { return tracker.Observe(probe, {1, 0}, now += 50, PollMs, 20); };
	(void)tick(); probe.hit.x += 5; probe.clearDepth += 5; (void)tick();
	Check(tick() == State::Open, "a finite recess starts a cue before tangent travel");
	const auto target = tracker.Current();
	for (int step = 0; step < 12; ++step) {
		++probe.hit.x;
		Check(tick() == State::Unknown && tracker.Accepts(target, State::Open),
			"ordinary wall drift neither repeats nor cancels a queued recess");
	}
	probe.hit.x -= 5; probe.clearDepth -= 5; (void)tick();
	Check(!tracker.Accepts(target, State::Open), "a real reverse edge invalidates the queued opening immediately");
	Check(tick() == State::Blocked, "the reverse edge confirms its closing without a repeat cooldown");
}
void ThinGaps() {
	for (int width : {1, 2, 3, 4}) {
		auto world = Field();
		for (int x = 0; x < 240; ++x)
			if (x < 118 || x >= 118+width) world.rooms[0].cells[90*240+x] = 1;
		Engine engine; (void)engine.Tick(world, 1000);
		std::array<int, DirectionCount> balance{};
		bool paired = true;
		for (uint64_t step = 1; step <= 15; ++step) {
			world.player.x += 4;
			const auto events = engine.Tick(world, 1000 + step*200);
			for (size_t i = 0; i < DirectionCount; ++i) {
				balance[i] += events[i] == State::Open ? 1 : events[i] == State::Blocked ? -1 : 0;
				paired &= balance[i] >= 0;
			}
		}
		Check(paired, "gaps too thin or too brief to announce cannot leave orphan closing cues");
	}
}
}
int main(int argc, char** argv) {
	const std::string_view test = argc > 1 ? argv[1] : "all";
	if (test == "all" || test == "speed") Speed();
	if (test == "all" || test == "encounter") Encounter();
	if (test == "all" || test == "range") RangeEdge();
	if (test == "all" || test == "wall") ContinuousWall();
	if (test == "all" || test == "niche") UnusableNiches();
	if (test == "all" || test == "recess") RecessAtSpeed();
	if (test == "all" || test == "coverage") PendingCoverageLoss();
	if (test == "all" || test == "drift") PendingWallDrift();
	if (test == "all" || test == "thin") ThinGaps();
	Passed();
}
