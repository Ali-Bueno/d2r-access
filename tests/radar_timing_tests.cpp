#include "radar.h"
#include "test_support.h"
#include <cmath>

using namespace D2RAccess::Radar;
namespace {
World Crossing() {
	World world;
	world.session = world.level = 1;
	world.player = {123, 87}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
	world.rooms.push_back({0, 0, 240, 240, std::vector<uint16_t>(240 * 240)});
	for (int y = 0; y < 240; ++y) for (int x = 0; x < 240; ++x)
		if ((x-y < 30 || x-y > 65) && (x+y == 206 || x+y == 214)) world.rooms[0].cells[y * 240 + x] = 1;
	return world;
}
Guidance Analyze(const World& world, uint64_t playback) {
	const Position up{-std::sqrt(0.5), -std::sqrt(0.5)};
	return EvaluateGuidance(world, up, Scan(world, up, {}, Layer::Terrain), Scan(world, up, {}), {12, -12}, {}, playback);
}
void Budgets() {
	AudioLoad load;
	Events candidates{}; candidates[0] = State::Open;
	Check(PlaybackBudget(load, candidates, 0) == CueDurationMs + AudioGuardMs, "a lone cue in idle audio does not reserve three nonexistent cues");
	candidates[3] = State::Open;
	Check(PlaybackBudget(load, candidates, 0) == CueDurationMs + AudioGuardMs + CueSlotMs,
		"simultaneous directions reserve serialized playback and the required silence");
	load.pending[3] = State::Blocked;
	Check(PlaybackBudget(load, candidates, 0) == CueDurationMs + AudioGuardMs + CueSlotMs,
		"replacement of a pending direction counts once");
	load.busy = true;
	Check(PlaybackBudget(load, candidates, 0) == CueDurationMs + AudioGuardMs + 2*CueSlotMs,
		"in-flight playback and a worker taking a pending cue have a full-slot allowance");
	candidates.fill(State::Open);
	Check(PlaybackBudget(load, candidates, 0) == CueDurationMs + AudioGuardMs + 4*CueSlotMs,
		"a busy four-direction batch accounts for all possible sounds before reaction");
}
void RunningExit() {
	auto world = Crossing();
	Events batch{}; batch[0] = batch[3] = State::Open;
	const auto quietBudget = PlaybackBudget({}, batch, 0);
	Check(Analyze(world, quietBudget).openingSafe, "a modest running junction is currently usable");
	constexpr uint64_t OldFixedBudget = 4*(CueDurationMs + CueGapMs); // deployed pre-fix policy
	Check(Analyze(world, OldFixedBudget).openingSafe, "audio queue length cannot hide existing geometry");
	AudioLoad busy; busy.busy = true; busy.pending.fill(State::Blocked);
	Check(Analyze(world, PlaybackBudget(busy, batch, 0)).openingSafe, "a busy queue changes delivery timing, not current passage classification");
	world.rooms[0].cells[80*240+130] = 1;
	Check(Analyze(world, quietBudget).openingSafe, "a post beyond the current turn does not invalidate that turn");
	world.rooms[0].cells[86*240+122] = 1;
	Check(!Analyze(world, quietBudget).openingSafe, "a collision inside the current turn still invalidates an opening");
	world = Crossing(); world.player = {127, 83};
	Check(Analyze(world, quietBudget).openingSafe, "an opening near its far edge remains audible while it is physically usable");
}
void EngineBatch() {
	auto world = Crossing(); world.player = {110,100};
	Engine engine; (void)engine.Tick(world,1000);
	unsigned openings = 0;
	for (uint64_t tick=1; tick<=36; ++tick) {
		const double x=110+0.6*tick, y=100-0.6*tick;
		world.player={std::floor(x),std::floor(y)}; world.subtileFraction={x-world.player.x,y-world.player.y};
		const auto events=engine.Tick(world,1000+tick*50);
		if (events[0]==State::Open) {
			++openings;
			Check(engine.Velocity().x > 10, "the exit is confirmed while still running");
			Check(engine.Sensors()[0].guidance.playbackMs >= CueDurationMs+AudioGuardMs+CueSlotMs,
				"the engine reserves both simultaneous side openings before committing either");
		}
	}
	Check(openings==1,"a newly usable running exit sounds once without requiring a stop");
}
}
int main() { Budgets(); RunningExit(); EngineBatch(); Passed(); }
