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
	Check(Silent(engine.Tick(world, 1000)), "initial player observation is silent");
	for (uint64_t tick = 1; tick <= 4; ++tick) {
		world.player.x += 1;
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "heading acquisition and baseline are silent");
	}
	Check(engine.Heading() == 0, "actual +X movement acquires the expected heading");
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
void TopologyTests() {
	auto world = Field();
	for (int x = 0; x < 400; ++x) {
		if (x >= 120 && x <= 130) continue;
		Put(world, x, 97, 1);
		Put(world, x, 103, 1);
	}
	Engine engine;
	Baseline(engine, world);
	Check(engine.Sensors()[1].stable == State::Blocked && engine.Sensors()[2].stable == State::Blocked, "straight corridor baselines both sides as blocked");
	unsigned leftOpen = 0, rightOpen = 0, leftClose = 0, rightClose = 0;
	bool simultaneous = false;
	for (uint64_t tick = 5; tick <= 35; ++tick) {
		world.player.x += 1;
		const auto events = engine.Tick(world, 1000 + tick * 50);
		leftOpen += events[1] == State::Open;
		rightOpen += events[2] == State::Open;
		leftClose += events[1] == State::Blocked;
		rightClose += events[2] == State::Blocked;
		simultaneous |= events[1] == State::Open && events[2] == State::Open;
		Check(events[0] == State::Unknown && events[3] == State::Unknown, "unchanged front and rear stay silent at crossroad");
	}
	Check(leftOpen == 1 && rightOpen == 1 && leftClose == 1 && rightClose == 1, "one opening and closing per side while passing a crossroad");
	Check(simultaneous, "both sides of a crossroad are delivered in one batch");
	for (uint64_t tick = 36; tick < 50; ++tick) Check(Silent(engine.Tick(world, 1000 + tick * 50)), "stationary corridor never repeats cues");
	// Rotation must not present the old front/rear as newly appeared side paths.
	for (uint64_t tick = 50; tick < 58; ++tick) {
		world.player.y -= 0.5;
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "ninety-degree heading rebase is silent");
	}
}
void StabilityTests() {
	auto world = Field();
	Engine engine;
	Baseline(engine, world);
	world.player = {200, 200};
	Check(Silent(engine.Tick(world, 1250)) && engine.Heading() == -1, "teleport discards old topology and heading");
	Baseline(engine, world);
	++world.level;
	Check(Silent(engine.Tick(world, 1250)) && engine.Heading() == -1, "new area is silently rebased");
	Baseline(engine, world);
	Check(Silent(engine.Tick(world, 2000)) && engine.Heading() == -1, "long pause cannot replay stale topology");
	world = Field();
	engine.Reset();
	Baseline(engine, world);
	for (uint64_t tick = 5; tick < 30; ++tick) {
		world.player.x += 1;
		world.player.y += tick % 2 ? 1 : -1;
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "joystick zig-zag never causes four fake topology cues");
	}
	world = Field();
	engine.Reset();
	Baseline(engine, world);
	for (int x = 0; x < 400; ++x) Put(world, x, 99, 1);
	Check(Silent(engine.Tick(world, 1250)), "a stationary raw collision change starts a candidate only");
	Check(Silent(engine.Tick(world, 1350)), "time alone cannot confirm stationary terrain changes");
	for (int x = 0; x < 400; ++x) Put(world, x, 99, 0);
	Check(Silent(engine.Tick(world, 1400)), "brief unstable blockage is discarded");
	world = Field();
	engine.Reset();
	(void)engine.Tick(world, 1000);
	for (uint64_t tick = 1; tick <= 12; ++tick) {
		if (tick % 2) world.player.x += 1;
		else world.player.y += 1;
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "integer diagonal staircase does not emit false transitions");
	}
	Check(engine.Heading() == 1, "alternating X/Y subtiles acquire a stable diagonal heading");
	world = Field();
	engine.Reset();
	Baseline(engine, world);
	const uint64_t revision = engine.Revision();
	world.session += 1;
	(void)engine.Tick(world, 1250);
	Check(engine.Revision() != revision, "session change invalidates audio already queued in the old frame");
	world = Field();
	engine.Reset();
	Baseline(engine, world);
	const auto originalRoom = world.rooms.front();
	world.rooms.front().width = 117;
	world.rooms.front().cells.resize(117 * 400);
	Check(Silent(engine.Tick(world, 1250)) && engine.Sensors()[0].stable == State::Unknown, "loss of nearby terrain drops the old state without a closing sound");
	world.rooms.front() = originalRoom;
	for (uint64_t tick = 6; tick <= 10; ++tick) {
		world.player.x += 0.5;
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "restored terrain silently establishes a new baseline");
	}
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
		else Check(left == right, "front and rear cues use centred stereo");
		Check(pcm.front() == 0 && std::abs(pcm.back()) < 50, "cue envelope avoids endpoint clicks");
	}
	events.fill(State::Open);
	const auto batch = RadarAudio::Synthesize(events);
	Check(std::none_of(batch.begin(), batch.end(), [](int s) { return std::abs(s) >= 32767; }), "simultaneous cues stay below clipping");
	Events front{}, rear{};
	front[0] = rear[3] = State::Open;
	Check(RadarAudio::Synthesize(front) != RadarAudio::Synthesize(rear), "rear has a distinct pitch");
	front[0] = State::Blocked;
	Check(RadarAudio::Synthesize(front) != RadarAudio::Synthesize(rear), "blocked tock differs from opening tin");
}
}
int main() {
	GridTests();
	TopologyTests();
	StabilityTests();
	AudioTests();
	Passed();
}
