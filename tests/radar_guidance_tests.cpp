#include "radar.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <chrono>

using namespace D2RAccess::Radar;
namespace {
World Field() {
	World world;
	world.session = world.level = 1;
	world.player = {110, 100};
	world.bodyPattern = 1;
	world.walkingMask = 0x1C09;
	world.rooms.push_back({0, 0, 400, 400, std::vector<uint16_t>(400 * 400)});
	return world;
}
void Put(World& world, int x, int y, uint16_t flags) { world.rooms[0].cells[y * 400 + x] = flags; }
bool Silent(const Events& events) { return std::all_of(events.begin(), events.end(), [](auto s) { return s == State::Unknown; }); }
World Crossroad(int first, int last) {
	auto world = Field();
	for (int y = 0; y < 400; ++y) for (int x = 0; x < 400; ++x) {
		if (x-y >= first && x-y <= last) continue;
		if (x+y == 206 || x+y == 214) Put(world, x, y, 1);
	}
	return world;
}
void OccupancyRegression() {
	auto world = Field(); Engine engine;
	(void)engine.Tick(world, 1000);
	for (uint64_t tick = 1; tick <= 80; ++tick) {
		// Native pattern 1 stamps MONSTER plus NO_PATH at the actor centre.
		// Each occupied/empty interval outlasts the old 250 ms confirmation.
		Put(world, 111, 101, (tick / 10) % 2 ? 0x1100 : 0);
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "moving creatures never announce alternating walls and exits");
	}
}
void FastNarrowOpening() {
	auto world = Crossroad(30, 50); Engine engine;
	(void)engine.Tick(world, 1000);
	for (uint64_t tick = 1; tick <= 35; ++tick) {
		world.player.x += 1; world.player.y -= 1;
		const auto events = engine.Tick(world, 1000 + tick * 50);
		Check(events[0] == State::Unknown && events[3] == State::Unknown,
			"a side gap passed before the reaction window produces neither an opening nor its closing partner");
	}
}
void CollisionLayers() {
	auto world = Field();
	for (const uint16_t actor : std::array<uint16_t, 3>{0x1100, 0x1080, 0x2100}) {
		world.walkingMask = 0x3D89; // include actor/pet bits as alternate native movement modes can
		Put(world, 111, 101, actor);
		Check(world.Cell(111, 101) == State::Blocked && world.Cell(111, 101, Layer::Terrain) == State::Open,
			"actor filtering changes only terrain estimates, never physical passage queries");
		for (const uint16_t solid : std::array<uint16_t, 4>{1, 8, 0x400, 0x800}) {
			Put(world, 111, 101, actor | solid);
			Check(world.Cell(111, 101, Layer::Terrain) == State::Blocked, "actor over wall, door, object or no-player cell preserves terrain");
		}
	}
	for (const uint16_t flags : std::array<uint16_t, 4>{0x1000, 0x2000, 0xC0, 0x1C0}) {
		Put(world, 111, 101, flags);
		Check(world.Cell(111, 101, Layer::Terrain) == State::Blocked, "standalone markers and water remain blocked, including monster over water");
	}
	Put(world, 111, 101, 0x1120);
	Check(world.Cell(111, 101, Layer::Terrain) == State::Unknown, "actor bits cannot turn missing terrain into open space");
}
Guidance Analyze(const World& world, Position axis, Position velocity) {
	return EvaluateGuidance(world, axis, Scan(world, axis, {}, Layer::Terrain), Scan(world, axis, {}), velocity, {});
}
void ReactionWindow() {
	const Position up{-std::sqrt(0.5), -std::sqrt(0.5)};
	auto world = Crossroad(30, 50); world.player = {123, 87};
	Check(Analyze(world, up, {}).openingSafe, "a body-wide passage is usable when standing in it");
	Check(Analyze(world, up, {2, -2}).openingSafe, "a slow walk leaves enough reaction time in a modest opening");
	Check(!Analyze(world, up, {20, -20}).openingSafe, "the same opening is too short at running speed");
	world.player = {118, 92};
	Check(!Analyze(world, up, {2, -2}).openingSafe, "an exit ahead cannot sound before turning is physically possible");
	world = Crossroad(30, 130); world.player = {123, 87};
	Check(Analyze(world, up, {20, -20}).openingSafe, "a wide intersection permits a reaction at running speed");
	// A thin post inside the forecast cannot be skipped by checking only its endpoints.
	Put(world, 130, 80, 1);
	Check(!Analyze(world, up, {20, -20}).openingSafe, "every intermediate footprint in the forecast is checked");
	world = Field();
	world.rooms[0].width = 120; world.rooms[0].cells.resize(120 * 400);
	Check(!Analyze(world, up, {20, -20}).openingSafe, "unknown coverage along continued movement rejects an opening promise");
	world = Field(); Put(world, 109, 99, 0x1100);
	Check(!Analyze(world, up, {}).openingSafe, "a monster occupying an otherwise clear opening never produces a tin");
}
double FirstWarning(uint64_t interval) {
	auto world = Field(); Engine engine;
	for (int y = 0; y < 400; ++y) if (300-y >= 0 && 300-y < 400) Put(world, 300-y, y, 1);
	(void)engine.Tick(world, 1000);
	for (uint64_t tick = 1; tick < 45; ++tick) {
		world.player.x += 1; world.player.y += 1;
		const auto events = engine.Tick(world, 1000 + tick * interval);
		if (events[3] == State::Blocked) {
			const auto v = engine.Velocity();
			const double distance = engine.Sensors()[3].terrain.clearDepth;
			Check(distance / std::hypot(v.x, v.y) >= 1.1, "wall warning leaves reaction and audio queue time before contact");
			return distance;
		}
	}
	Check(false, "approaching a wall must warn before collision");
	return 0;
}
void SpeedAndBoundary() {
	const double slow = FirstWarning(150), fast = FirstWarning(50);
	Check(fast > slow, "running warns farther from the same wall than walking");
	auto world = Field(); Engine engine;
	for (int y = 0; y < 400; ++y) if (214-y >= 0 && 214-y < 400) Put(world, 214-y, y, 1);
	(void)engine.Tick(world, 1000);
	for (uint64_t tick = 1; tick <= 12; ++tick) {
		world.player.x -= 1; world.player.y -= 1;
		Check(engine.Tick(world, 1000 + tick * 100)[3] != State::Open, "retreating from the same world obstacle is not a new exit");
	}
	Check(std::hypot(engine.Velocity().x, engine.Velocity().y) > 0, "velocity derives from actual subtile displacement");
	(void)engine.Tick(world, 2300); (void)engine.Tick(world, 2400);
	Check(engine.Velocity().x == 0 && engine.Velocity().y == 0, "velocity settles to zero after stopping");
	world.player = {200, 200}; (void)engine.Tick(world, 2450);
	Check(engine.Velocity().x == 0 && engine.Velocity().y == 0, "teleports never create a fictitious running speed");
}
void StopAtOpening() {
	auto world = Crossroad(30, 50); Engine engine;
	(void)engine.Tick(world, 1000);
	for (uint64_t tick = 1; tick <= 13; ++tick) {
		world.player.x += 1; world.player.y -= 1;
		Check(engine.Tick(world, 1000 + tick * 50)[0] != State::Open, "fast entry does not promise a fleeting opening");
	}
	unsigned openings = 0;
	for (uint64_t tick = 14; tick <= 35; ++tick) openings += engine.Tick(world, 1000 + tick * 50)[0] == State::Open;
	Check(openings == 1, "stopping at a small usable passage announces it once");
}
void OccupiedDoor() {
	auto world = Field(); Engine engine;
	Put(world, 111, 101, 0x800);
	(void)engine.Tick(world, 1000);
	Put(world, 111, 101, 0x1100);
	for (uint64_t tick = 1; tick <= 12; ++tick)
		Check(engine.Tick(world, 1000 + tick * 50)[3] == State::Unknown, "opening a door onto an occupied threshold cannot announce free passage");
	Put(world, 111, 101, 0);
	unsigned openings = 0;
	for (uint64_t tick = 13; tick <= 30; ++tick) openings += engine.Tick(world, 1000 + tick * 50)[3] == State::Open;
	Check(openings == 1, "a pending door opening can sound once the threshold actually clears");
}
void LimitedCorridorOpening() {
	auto world = Field();
	for (int y = 0; y < 400; ++y) for (int x = 0; x < 400; ++x) {
		if (x-y >= 30 && x-y <= 130) continue;
		if (x+y == 202 || x+y == 218) Put(world, x, y, 1);
	}
	Engine engine; (void)engine.Tick(world, 1000);
	Check(engine.Sensors()[0].probe.available && engine.Sensors()[0].probe.observed == State::Unknown,
		"limited clearance is distinguishable from an unloaded boundary");
	unsigned openings = 0;
	for (uint64_t tick = 1; tick <= 35; ++tick) {
		world.player.x += 1; world.player.y -= 1;
		openings += engine.Tick(world, 1000 + tick * 50)[0] == State::Open;
	}
	Check(openings == 1, "a useful exit from limited space does not require first touching the wall");
}
void AccelerationAndTurns() {
	auto world = Crossroad(30, 50); world.player = {119, 91}; Engine engine;
	for (uint64_t tick = 0; tick < 6; ++tick) (void)engine.Tick(world, 1000 + tick * 50);
	for (uint64_t tick = 6; tick < 15; ++tick) {
		world.player.x += 1; world.player.y -= 1;
		Check(engine.Tick(world, 1000 + tick * 50)[0] != State::Open, "accelerating from rest at a narrow doorway cannot promise a fleeting exit");
	}
	world = Field(); engine.Reset(); (void)engine.Tick(world, 1000);
	for (uint64_t tick = 1; tick <= 6; ++tick) {
		world.player.x += 1; world.player.y -= 1;
		(void)engine.Tick(world, 1000 + tick * 50);
	}
	for (uint64_t tick = 7; tick <= 8; ++tick) {
		world.player.x -= 1; world.player.y -= 1;
		(void)engine.Tick(world, 1000 + tick * 50);
	}
	Check(engine.Velocity().x < 0 && engine.Velocity().y < 0, "forecast follows a changed movement direction within its confirmation interval");
}
void SubtileMotion() {
	auto world = Field(); Engine engine;
	const auto position = [&](double x, double y) {
		world.player = {std::floor(x), std::floor(y)};
		world.subtileFraction = {x - world.player.x, y - world.player.y};
	};
	position(110.875, 100.25); (void)engine.Tick(world, 1000);
	bool smooth = true;
	for (uint64_t tick = 1; tick <= 24; ++tick) {
		// Constant 2.5 / 1.25 subtiles per second, crossing the X and Y cell
		// boundaries on different ticks. Integer-only motion invents turns.
		position(110.875 + tick * 0.125, 100.25 + tick * 0.0625);
		(void)engine.Tick(world, 1000 + tick * 50);
		const auto velocity = engine.Velocity();
		smooth &= std::abs(velocity.x - 2.5) < 0.0001 && std::abs(velocity.y - 1.25) < 0.0001;
	}
	Check(smooth, "constant subcell travel preserves speed and direction across unequal cell crossings");
	(void)engine.Tick(world, 2250); (void)engine.Tick(world, 2350); (void)engine.Tick(world, 2450);
	Check(engine.Velocity().x == 0 && engine.Velocity().y == 0, "fractional position settles to zero speed after a real stop");
	engine.Reset(); position(110.25, 100.5); (void)engine.Tick(world, 3000);
	for (uint64_t tick = 1; tick <= 6; ++tick) {
		position(110.25 + tick * 0.0625, 100.5);
		(void)engine.Tick(world, 3000 + tick * 50);
	}
	Check(world.player.x == 110 && engine.Velocity().x > 1,
		"movement inside one cell is not mistaken for standing still after 200 milliseconds");
	position(110.5625, 100.5); (void)engine.Tick(world, 3350);
	position(110.5, 100.5); (void)engine.Tick(world, 3400);
	Check(world.player.x == 110 && engine.Velocity().x < 0,
		"a turn inside the same collision cell changes the forecast direction");
	position(200.25, 200.5); (void)engine.Tick(world, 3450);
	Check(engine.Velocity().x == 0 && engine.Velocity().y == 0, "fractional teleport still resets motion");
}
void ProfileQueries() {
	// Optional manual timing: comparable room count to the live camp (35),
	// with the containing room last to exercise room lookup's expensive case.
	auto world = Field();
	for (int i = 0; i < 34; ++i) world.rooms.insert(world.rooms.begin(), {500+i, 500, 1, 1, {0}});
	Engine engine; (void)engine.Tick(world, 1000);
	std::vector<double> elapsed;
	for (uint64_t tick = 1; tick <= 60; ++tick) {
		world.player.x += 1; world.player.y += 1;
		const auto start = std::chrono::steady_clock::now();
		Check(Silent(engine.Tick(world, 1000 + tick * 50)), "35-room open field remains silent while moving");
		elapsed.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
	}
	std::sort(elapsed.begin(), elapsed.end());
	std::cout << "35-room engine timing: median=" << elapsed[elapsed.size()/2] << " ms, max=" << elapsed.back() << " ms\n";
}
}
int main(int argc, char**) {
	if (argc > 1) { ProfileQueries(); Passed(); return 0; }
	AccelerationAndTurns();
	SubtileMotion();
	CollisionLayers();
	ReactionWindow();
	SpeedAndBoundary();
	StopAtOpening();
	OccupiedDoor();
	LimitedCorridorOpening();
	OccupancyRegression();
	FastNarrowOpening();
	Passed();
}
