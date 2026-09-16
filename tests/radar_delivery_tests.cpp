#include "radar.h"
#include "radar_audio.h"
#include "test_support.h"
#include <algorithm>
#include <cmath>

using namespace D2RAccess;
using namespace Radar;
namespace {
World Field() {
	World world;
	world.session = world.level = 1;
	world.player = {110, 100}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
	world.rooms.push_back({0, 0, 240, 240, std::vector<uint16_t>(240 * 240)});
	return world;
}
void Put(World& world, int x, int y, uint16_t flags) { world.rooms[0].cells[y * 240 + x] = flags; }
bool Silent(const Events& events) { return std::all_of(events.begin(), events.end(), [](auto s) { return s == State::Unknown; }); }
World Crossing(bool vertical) {
	auto world = Field();
	if (vertical) world.player.y = 110;
	for (int y = 0; y < 240; ++y) for (int x = 0; x < 240; ++x)
		if (const int reflectedY = vertical ? 210-y : y;
			(x-reflectedY < 30 || x-reflectedY > 65) && (x+reflectedY == 206 || x+reflectedY == 214)) Put(world, x, y, 1);
	return world;
}
void Backtrack() {
	auto world = Field();
	for (int y = 0; y < 240; ++y) if (214-y >= 0 && 214-y < 240) Put(world, 214-y, y, 1);
	Engine engine; (void)engine.Tick(world, 1000);
	uint64_t now = 1000;
	for (unsigned trip = 0; trip < 2; ++trip) {
		unsigned open = 0, closed = 0;
		for (unsigned step = 0; step < 12; ++step) {
			world.player.x -= 1; world.player.y -= 1; now += 100;
			open += engine.Tick(world, now)[3] == State::Open;
		}
		Check(open == 1, "walking away updates the same wall direction to open on every trip");
		for (unsigned step = 0; step < 12; ++step) {
			world.player.x += 1; world.player.y += 1; now += 100;
			closed += engine.Tick(world, now)[3] == State::Blocked;
		}
		Check(closed == 1, "returning to the same wall announces blocked again on every trip");
	}
	for (unsigned step = 0; step < 10; ++step) Check(Silent(engine.Tick(world, now += 50)), "unchanged wall stays silent after returning");
}
void QueueTime() {
	RadarAudio::CueQueue queue;
	Events pair{}; pair[0] = pair[3] = State::Open;
	queue.Push(pair, {.eventMs = 1000});
	const auto first = queue.Take();
	auto load = queue.Playback(first, 50, 0);
	Check(load.remainingMs[0] == 120 && load.remainingMs[3] == 340, "a pending second side uses remaining playback, gap and its own duration");
	Check(PlaybackBudget(load, {}, 3) == 340, "validation does not restart a full-batch horizon for the queued side");
	Events newSide{}; newSide[1] = State::Open;
	Check(PlaybackBudget(load, newSide, 3) == 340 + CueSlotMs, "a newly arriving direction still reserves a possible intervening cue");
	load = queue.Playback({}, 0, CueGapMs);
	Check(load.remainingMs[3] == CueGapMs + CueDurationMs + AudioGuardMs, "the actual silent gap is included after the first cue finishes");
	const auto second = queue.Take();
	Check(second[3] == State::Open && queue.Empty(), "the unchanged second opening remains queued independently");
	load = queue.Playback(second, CueDurationMs + AudioGuardMs + 100, 0);
	Check(load.remainingMs[3] == AudioGuardMs, "late device completion retains a guard without elapsed-time underflow");
}
void DeliveryRetry() {
	auto world = Field(); Put(world, 111, 101, 0x800);
	Engine engine; (void)engine.Tick(world, 1000);
	Put(world, 111, 101, 0); (void)engine.Tick(world, 1050);
	Check(engine.Tick(world, 1100)[3] == State::Open, "door opening enters the sound queue");
	Put(world, 111, 101, 0x1100); // occupied after generation, before playback
	(void)engine.Tick(world, 1150);
	Check(engine.Sensors()[3].cueState == State::Unknown, "an occupied opening is immediately invalid for queued playback");
	AudioLoad failed; failed.failed[3] = {State::Open, 1100};
	Check(Silent(engine.Tick(world, 1200, failed)), "failed delivery is not retried into an occupied passage");
	Put(world, 111, 101, 0);
	unsigned retries = 0;
	uint64_t retryTime = 0;
	for (uint64_t now = 1250; now <= 1600; now += 50) {
		if (engine.Tick(world, now)[3] == State::Open) { ++retries; retryTime = now; }
	}
	Check(retries == 1 && retryTime >= 1350, "a discarded opening is retried once when safe, with a bounded retry rate");
	Check(Silent(engine.Tick(world, 1650, failed)), "late failure of an old event cannot rearm its replacement");
	Put(world, 111, 101, 0x800); (void)engine.Tick(world, 1700);
	Check(engine.Tick(world, 1750)[3] == State::Blocked, "a real closure updates the previously open direction");
	failed.failed[3] = {State::Open, retryTime};
	Check(Silent(engine.Tick(world, 1800, failed)), "a stale open failure cannot overwrite the newer closed state");
	++world.session;
	Check(Silent(engine.Tick(world, 1850, failed)), "session rebasing discards old delivery failures");
}
void RunningPair(bool vertical, bool finite = false) {
	auto world = Crossing(vertical); Engine engine;
	if (finite) {
		world.player = {110, 100};
		// Two finite side walls recede and return while both old and new
		// distances still exceed the eight-subtile passability threshold.
		for (int y=0; y<240; ++y) for (int x=0; x<240; ++x) {
			const int axis = vertical ? x-y-10 : x+y-210;
			Put(world, x, y, std::abs(axis) == 26 || std::abs(axis) == 16 ? 1 : 0);
		}
	}
	RadarAudio::CueQueue queue;
	Events playing{};
	RadarAudio::CueSource source{};
	std::array<CueFailure, DirectionCount> failed{};
	std::array<unsigned, DirectionCount> completed{}, closed{};
	uint64_t started = 0, nextCue = 0;
	unsigned discarded = 0;
	// Run the real engine/queue policies together. Audio advances every 10 ms;
	// terrain polls every 50 ms, and both 120 ms tones retain the 50 ms gap.
	for (uint64_t now = 1000; now <= 7300; now += 10) {
		if (!Silent(playing) && now - started >= CueDurationMs) {
			for (size_t i = 0; i < DirectionCount; ++i) {
				if (playing[i] == State::Open) ++completed[i];
				if (playing[i] == State::Blocked) ++closed[i];
			}
			playing = {}; nextCue = now + Radar::CueGapMs;
		}
		if ((now - 1000) % PollMs == 0) {
			const uint64_t elapsed = now - 1000;
			if (finite) {
				const bool recessed = (elapsed >= 500 && elapsed < 1500) || (elapsed >= 2500 && elapsed < 3500);
				for (int y=0; y<240; ++y) for (int x=0; x<240; ++x) {
					const int axis = vertical ? x-y-10 : x+y-210;
					if (std::abs(axis) == 16) Put(world, x, y, recessed ? 0 : 1);
				}
			} else {
				const double advance = 12.0 * (elapsed <= 3000 ? elapsed : elapsed < 6000 ? 6000-elapsed : 0) / 1000;
				const double x = 110 + advance, y = vertical ? 110 + advance : 100 - advance;
				world.player = {std::floor(x), std::floor(y)}; world.subtileFraction = {x-world.player.x, y-world.player.y};
			}
			auto load = queue.Playback(playing, now - started, nextCue > now ? nextCue - now : 0);
			load.failed = failed; failed = {};
			const auto events = engine.Tick(world, now, load);
			Events valid{};
			for (size_t i = 0; i < DirectionCount; ++i) {
				const auto& sensor = engine.Sensors()[i]; valid[i] = sensor.cueState;
				if (queue.Pending()[i] != State::Unknown && queue.Pending()[i] != valid[i]) {
					failed[i] = {queue.Pending()[i], queue.Source(i).eventMs}; ++discarded;
				}
				const auto audible = sensor.cueState == State::Blocked ? State::Blocked : sensor.probe.observed;
				if (playing[i] != State::Unknown && playing[i] != audible) {
					failed[i] = {playing[i], source.eventMs}; playing = {}; nextCue = now + Radar::CueGapMs; ++discarded;
				}
			}
			queue.Retain(valid); queue.Push(events, {.eventMs = now});
		}
		if (Silent(playing) && now >= nextCue && !queue.Empty()) { playing = queue.Take(&source); started = now; }
	}
	const size_t firstSide = vertical ? 1 : 0, secondSide = vertical ? 2 : 3;
	Check(completed[firstSide] == 2 && completed[secondSide] == 2, "both sides of a running junction finish their openings on the outbound and return trips");
	Check(closed[firstSide] == 2 && closed[secondSide] == 2, "both sides announce the walls again when leaving the junction in either direction");
	Check(discarded == 0, "the queued side does not invalidate itself by restarting its audio forecast");
}
}
int main() { Backtrack(); QueueTime(); DeliveryRetry(); RunningPair(false); RunningPair(true); RunningPair(false, true); RunningPair(true, true); Passed(); }
