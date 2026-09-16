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
	world.session = 1;
	world.level = 1;
	world.player = {110, 100};
	world.bodyPattern = 1;
	world.walkingMask = 0x1C09; // observed native player walking mask
	world.rooms.push_back({0, 0, 400, 400, std::vector<uint16_t>(400 * 400)});
	return world;
}
void Put(World& world, int x, int y, uint16_t value) {
	auto& room = world.rooms.front();
	room.cells[static_cast<size_t>((y - room.y) * room.width + x - room.x)] = value;
}
bool Silent(const Events& events) { return std::all_of(events.begin(), events.end(), [](State s) { return s == State::Unknown; }); }
void Baseline(Engine& engine, World& world) {
	Check(Silent(engine.Tick(world, 1000)), "first screen-axis baseline is silent without walking");
	for (uint64_t tick = 1; tick <= 4; ++tick)
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "unchanged terrain at rest stays silent");
}
void GridTests() {
	auto world = Field();
	Check(world.Cell(-1, 100) == State::Unknown, "unloaded cells are unknown");
	Put(world, 110, 100, 0x180); // ordinary player + monster presence
	Check(world.Cell(110, 100) == State::Open, "moving actors do not change terrain topology");
	Put(world, 110, 100, 0x800);
	Check(world.Cell(110, 100) == State::Blocked, "closed native door blocks walking");
	Put(world, 110, 100, 0);
	Check(world.Cell(110, 100) == State::Open, "open native door is reflected by the next snapshot");
	Put(world, 110, 100, 0x27);
	Check(world.Cell(110, 100) == State::Unknown, "engine's absent-space mask does not fabricate a wall");
	Put(world, 110, 100, 0);
	Put(world, 111, 100, 1);
	Check(world.Body(110, 100) == State::Blocked, "body footprint rejects centre-clear side obstruction");
	world.bodyPattern = 0;
	Check(world.Body(110, 100) == State::Open, "native point pattern uses only its own cell");
	Put(world, 111, 100, 0);
	Put(world, 111, 101, 1);
	world.bodyPattern = 1;
	Check(world.Body(110, 100) == State::Open, "native cross pattern excludes diagonal corners");
	world.bodyPattern = 2;
	Check(world.Body(110, 100) == State::Blocked, "native square pattern includes diagonal corners");
	world.bodyPattern = 1;
	Put(world, 111, 101, 0);
	for (int y = 0; y < 400; ++y) if (y != 100) Put(world, 112, y, 1);
	Check(Scan(world, {1, 0}, {}).observed == State::Blocked, "thin ray through a body-width obstruction cannot announce open");
	world = Field();
	auto probe = Scan(world, {1, 0}, {});
	Check(probe.observed == State::Open && probe.depths[1] == 150, "open field samples the configured far range");
	world.rooms.front().width = 115;
	world.rooms.front().cells.resize(115 * 400);
	probe = Scan(world, {1, 0}, {});
	Check(probe.observed == State::Unknown && !probe.available, "an early unloaded boundary is neither open nor blocked");
}
void SymmetryTests() {
	constexpr std::array<Position, 8> steps{{{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}}};
	for (size_t heading = 0; heading < steps.size(); ++heading) {
		auto world = Field();
		world.player = {200, 200};
		for (int cell = 0; cell < 400; ++cell) {
			Put(world, 194, cell, 1); Put(world, 206, cell, 1);
			Put(world, cell, 194, 1); Put(world, cell, 206, 1);
		}
		const auto step = steps[heading];
		const double norm = std::hypot(step.x, step.y);
		const auto probe = Scan(world, {step.x / norm, step.y / norm}, {});
		Check(probe.depths[1] == (heading % 2 == 0 ? 5.0 : 7.5), "point rays remain symmetric in all eight directions");
		Check(probe.clearDepth == (heading % 2 == 0 ? 4.0 : 6.0), "full-body clearance remains symmetric in all eight directions");
	}
	for (size_t heading = 0; heading < steps.size(); heading += 2) {
		auto world = Field(); world.player = {200, 200}; world.bodyPattern = 0;
		const auto step = steps[heading];
		for (int cell = 0; cell < 400; ++cell) {
			if (step.x != 0) { Put(world, cell, 199, 1); Put(world, cell, 201, 1); }
			else { Put(world, 199, cell, 1); Put(world, 201, cell, 1); }
		}
		Check(Scan(world, step, {}).depths[1] == Settings{}.farRange, "cardinal rays stay in their own grid row or column");
	}
}
void ScreenAxisTests() {
	// World diagonals projected by the installed game's X-Y / X+Y transform.
	constexpr std::array<Position, 4> steps{{{-1, -1}, {-1, 1}, {1, -1}, {1, 1}}};
	for (size_t direction = 0; direction < steps.size(); ++direction) {
		auto world = Field(); const auto step = steps[direction];
		Put(world, 110 + int(step.x) * 2, 100 + int(step.y) * 2, 1);
		Engine engine; Baseline(engine, world);
		for (size_t i = 0; i < DirectionCount; ++i)
			Check(engine.Sensors()[i].probe.observed == (i == direction ? State::Blocked : State::Open),
				"a screen-axis obstacle is assigned to the matching stick direction at rest");
		uint64_t now = 1200;
		const uint64_t revision = engine.Revision();
		for (const auto movement : steps) {
			world.player = {110 + movement.x, 100 + movement.y};
			(void)engine.Tick(world, now += 50);
			world.player = {110, 100};
			(void)engine.Tick(world, now += 50);
			Check(engine.Sensors()[direction].probe.observed == State::Blocked,
				"returning from any movement keeps the same screen-axis obstacle");
		}
		Check(engine.Revision() == revision, "turning never rotates the reference or cancels screen-axis cues");
	}
}
void TopologyTests() {
	auto world = Field();
	// Horizontal on screen: x+y is constant. A wide vertical side opening
	// interrupts both banks of this corridor at x-y in [30, 130]. This gives
	// enough time for a whole audio batch and reaction at this running speed.
	for (int y = 0; y < 400; ++y) for (int x = 0; x < 400; ++x) {
		if (x-y >= 30 && x-y <= 130) continue;
		if (x+y == 206 || x+y == 214) Put(world, x, y, 1);
	}
	Engine engine; Baseline(engine, world);
	Check(engine.Sensors()[0].stable == State::Blocked && engine.Sensors()[3].stable == State::Blocked,
		"horizontal screen corridor has blocked up/down sides");
	unsigned upOpen = 0, downOpen = 0, upClose = 0, downClose = 0;
	bool simultaneous = false;
	for (uint64_t tick = 5; tick <= 85; ++tick) {
		world.player.x += 1; world.player.y -= 1;
		const auto events = engine.Tick(world, 1000 + tick * 50);
		upOpen += events[0] == State::Open; downOpen += events[3] == State::Open;
		upClose += events[0] == State::Blocked; downClose += events[3] == State::Blocked;
		simultaneous |= events[0] == State::Open && events[3] == State::Open;
		Check(events[1] == State::Unknown && events[2] == State::Unknown, "unchanged left/right remain silent at a screen-axis crossroad");
	}
	Check(upOpen == 1 && downOpen == 1 && upClose == 1 && downClose == 1, "one opening and closing per side while passing a crossroad");
	Check(simultaneous, "both sides of a crossroad reach the audio queue");
	for (uint64_t tick = 86; tick < 100; ++tick)
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "stationary corridor never repeats cues");
}
void StabilityTests() {
	auto world = Field(); Engine engine; Baseline(engine, world);
	auto revision = engine.Revision();
	world.player = {200, 200};
	Check(Silent(engine.Tick(world, 1250)) && engine.Revision() != revision, "teleport cancels old cues and silently baselines the destination");
	revision = engine.Revision(); ++world.level;
	Check(Silent(engine.Tick(world, 1300)) && engine.Revision() != revision, "new area silently cancels old cues");
	revision = engine.Revision();
	Check(Silent(engine.Tick(world, 2000)) && engine.Revision() != revision, "long pause cannot replay stale topology");
	world = Field(); engine.Reset(); Baseline(engine, world);
	for (uint64_t tick = 5; tick < 30; ++tick) {
		world.player.x += 1; world.player.y += tick % 2 ? 1 : -1;
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "zig-zag in an open field does not cause topology events");
	}
	world = Field(); engine.Reset(); Baseline(engine, world);
	Put(world, 111, 101, 0x800);
	Check(Silent(engine.Tick(world, 1250)), "stationary door closure starts a candidate");
	Check(engine.Tick(world, 1300)[3] == State::Blocked, "a closed stationary door is announced on the next radar poll");
	Check(Silent(engine.Tick(world, 1350)), "the same blocked state is announced only once");
	Put(world, 111, 101, 0);
	Check(Silent(engine.Tick(world, 1400)), "one opening snapshot alone is not confirmed");
	Check(engine.Tick(world, 1450)[3] == State::Open, "an opposite opening is not delayed by a previous warning's cooldown");
	Put(world, 111, 101, 0x800);
	(void)engine.Tick(world, 1500);
	Check(engine.Tick(world, 1550)[3] == State::Blocked, "closing the same door again announces its changed state");
	Put(world, 111, 101, 0); (void)engine.Tick(world, 1600);
	Put(world, 111, 101, 0x800);
	Check(Silent(engine.Tick(world, 1650)), "single-poll flicker remains silent");
	world = Field(); engine.Reset(); Baseline(engine, world);
	revision = engine.Revision(); ++world.session;
	(void)engine.Tick(world, 1250);
	Check(engine.Revision() != revision, "session change invalidates queued audio");
	world = Field(); engine.Reset(); Baseline(engine, world);
	const auto rooms = world.rooms; world.rooms.clear();
	Check(Silent(engine.Tick(world, 1250)) && engine.Sensors()[0].stable == State::Unknown, "missing terrain drops the old state without inventing a wall");
	world.rooms = rooms;
	for (uint64_t tick = 6; tick <= 10; ++tick)
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "restored terrain silently establishes a baseline");
}
void AudioTests() {
	Events events{};
	const auto silent = RadarAudio::Synthesize(events);
	Check(std::all_of(silent.begin(), silent.end(), [](auto s) { return s == 0; }), "no event means silent PCM");
	for (size_t direction = 0; direction < DirectionCount; ++direction) {
		events = {};
		events[direction] = State::Open;
		const auto pcm = RadarAudio::Synthesize(events);
		double left = 0, right = 0;
		for (size_t i = 0; i < pcm.size(); i += 2) { left += double(pcm[i]) * pcm[i]; right += double(pcm[i + 1]) * pcm[i + 1]; }
		Check(left + right > 0, "each direction synthesizes an audible cue");
		if (direction == 1) Check(left > right * 10, "left cue is strongly left-panned");
		else if (direction == 2) Check(right > left * 10, "right cue is strongly right-panned");
		else Check(left == right, "up and down cues use centred stereo");
		Check(pcm.front() == 0 && std::abs(pcm.back()) < 50, "cue envelope avoids endpoint clicks");
	}
	events.fill(State::Open);
	const auto batch = RadarAudio::Synthesize(events);
	Check(std::none_of(batch.begin(), batch.end(), [](int s) { return std::abs(s) >= 32767; }), "simultaneous cues stay below clipping");
	Events front{}, rear{};
	front[0] = rear[3] = State::Open;
	Check(RadarAudio::Synthesize(front) != RadarAudio::Synthesize(rear), "down has a distinct pitch");
	front[0] = State::Blocked;
	Check(RadarAudio::Synthesize(front) != RadarAudio::Synthesize(rear), "blocked tock differs from opening tin");
}
void PassageRegressionTests() {
	auto world = Field();
	Put(world, 109, 100, 1);
	Check(Scan(world, {std::sqrt(0.5), std::sqrt(0.5)}, {}).observed == State::Open,
		"a clear destination allows moving away from an obstacle overlapping the current footprint");
	world = Field();
	// A clear centre ray and two clear lanes are insufficient: the player's
	// right shoulder hits this wall beyond the old two-subtile body sweep.
	for (int x = 114; x < 125; ++x) Put(world, x, 101, 1);
	Check(Scan(world, {1, 0}, {}).observed != State::Open,
		"an opening must fit the player's body throughout its claimed depth");
	world = Field();
	for (int x = 113; x < 125; ++x) Put(world, x, 101, 1);
	Check(Scan(world, {1, 0}, {}).observed == State::Blocked,
		"a shoulder obstruction within blocking distance is blocked even with a clear centre ray");
	Events pair{};
	pair[1] = pair[2] = State::Open;
	Events one{}; one[1] = State::Open;
	const auto single = RadarAudio::Synthesize(one), sequence = RadarAudio::Synthesize(pair);
	const size_t gap = RadarAudio::SampleRate * RadarAudio::Channels * 50 / 1000;
	Check(sequence.size() == 2 * single.size() + gap,
		"two simultaneous cues play successively with a 50 ms silent gap");
	Check(std::equal(single.begin(), single.end(), sequence.begin()), "the first cue is not mixed with the second");
	Check(std::all_of(sequence.begin() + single.size(), sequence.begin() + single.size() + gap,
		[](auto value) { return value == 0; }), "the interval between cues is silent");
}
void QueueTests() {
	RadarAudio::CueQueue queue;
	Events openings{}; openings.fill(State::Open);
	queue.Push(openings);
	for (size_t direction = 0; direction < DirectionCount; ++direction) {
		Events expected{}; expected[direction] = State::Open;
		Check(queue.Take() == expected, "simultaneous cues leave the queue individually in screen order");
	}
	Check(queue.Empty() && Silent(queue.Take()), "drained queue cannot repeat cues");
	queue.Push(openings);
	Events newest = openings; newest[1] = State::Blocked; newest[3] = State::Unknown;
	queue.Retain(newest);
	Events changes{}; changes[1] = State::Blocked; queue.Push(changes);
	Check(queue.Take()[0] == State::Open, "a still-valid opening is retained");
	Check(queue.Take()[1] == State::Blocked, "a queued opening is replaced by the newer block");
	Check(queue.Take()[2] == State::Open && queue.Empty(), "unknown or disappeared openings never reach playback");
	queue.Push(openings); queue.Retain({});
	Check(queue.Empty(), "terrain loss cancels every queued direction");
}
void QueueSourceTests() {
	RadarAudio::CueQueue queue;
	const RadarAudio::CueSource first{.eventMs = 1000, .session = 7, .level = 1, .position = {110, 100}};
	const RadarAudio::CueSource second{.eventMs = 1100, .session = 7, .level = 1, .position = {112, 102}};
	RadarAudio::CueSource taken{};
	Events pair{}; pair[1] = pair[2] = State::Open;
	queue.Push(pair, first);
	Check(queue.Take(&taken)[1] == State::Open && taken.eventMs == first.eventMs,
		"the first serial cue keeps its generated event ID");
	Events up{}; up[0] = State::Blocked;
	queue.Push(up, second);
	Check(queue.Take(&taken)[2] == State::Open && taken.eventMs == first.eventMs,
		"a newer direction cannot relabel an older queued cue");
	Check(taken.position.x == 110 && taken.position.y == 100 && taken.session == 7 && taken.level == 1,
		"queued cue context is the original position and session");
	Check(queue.Take(&taken)[0] == State::Blocked && taken.eventMs == second.eventMs,
		"round-robin playback reports the matching source rather than the last popped source");
	queue.Push(pair, first);
	Events closing{}; closing[1] = State::Blocked;
	queue.Push(closing, second);
	Check(queue.Pending()[1] == State::Blocked && queue.Source(1).eventMs == second.eventMs,
		"replacing a pending direction also replaces its trace ID");
	Check(queue.Pending()[2] == State::Open && queue.Source(2).eventMs == first.eventMs,
		"replacement preserves the unrelated pending direction's trace ID");
	Events observations{}; observations[1] = State::Blocked;
	queue.Retain(observations);
	Check(queue.Pending()[2] == State::Unknown, "invalidated directions no longer appear in pending traces");
	Check(queue.Take(&taken)[1] == State::Blocked && taken.eventMs == second.eventMs,
		"retaining a valid cue preserves its replacement source");
	Check(Silent(queue.Take(&taken)) && taken.eventMs == 0,
		"an empty dequeue clears its output source instead of attributing a phantom cue");
	queue.Push(pair, first);
	queue = {};
	Check(Silent(queue.Take(&taken)) && taken.eventMs == 0, "silencing the queue clears states and source data together");
	queue.Push(up, {.eventMs = 1200, .audition = true});
	Check(queue.Take(&taken)[0] == State::Blocked && taken.audition && taken.eventMs == 1200,
		"audition cues retain their test source and event ID");
}
}
int main() {
	QueueSourceTests();
	QueueTests();
	PassageRegressionTests();
	GridTests();
	SymmetryTests();
	ScreenAxisTests();
	TopologyTests();
	StabilityTests();
	AudioTests();
	Passed();
}
