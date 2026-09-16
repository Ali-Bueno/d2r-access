#include "radar.h"
#include "radar_readout.h"
#include "test_support.h"
#include <algorithm>
#include <cmath>

using namespace D2RAccess::Radar;
namespace {
World Field() {
	World world;
	world.session = world.level = 1;
	world.player = {100, 100}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
	world.rooms.push_back({0, 0, 240, 240, std::vector<uint16_t>(240 * 240)});
	return world;
}
void Wall(World& world, int sum, uint16_t flags) {
	for (int y = 0; y < 240; ++y) if (sum-y >= 0 && sum-y < 240) world.rooms[0].cells[y*240 + sum-y] = flags;
}
bool Silent(const Events& events) { return std::all_of(events.begin(), events.end(), [](auto s) { return s == State::Unknown; }); }
void FiniteOpening() {
	auto world = Field(); Wall(world, 216, 1); Wall(world, 226, 1);
	Engine engine; (void)engine.Tick(world, 1000);
	Check(engine.Sensors()[3].probe.observed == State::Open, "the near wall leaves usable space before the junction");
	Wall(world, 216, 0);
	Check(Silent(engine.Tick(world, 1050)), "a recessed boundary needs a second observation");
	Check(engine.Tick(world, 1100)[3] == State::Open, "receding to a farther wall announces an opening even when both distances are open");
	Check(Silent(engine.Tick(world, 1150)), "the farther wall is not repeated while standing still");
	Wall(world, 216, 1); (void)engine.Tick(world, 1200);
	Check(engine.Tick(world, 1250)[3] == State::Blocked, "returning to the near wall announces closing even while it remains farther than the blocked threshold");
	Check(Silent(engine.Tick(world, 1300)), "the returned boundary is announced only once");
}
void TurnBeforeWall() {
	auto world = Field(); Wall(world, 206, 1);
	const Position left{-std::sqrt(0.5), std::sqrt(0.5)};
	const auto movement = Scan(world, left, {});
	Check(movement.observed == State::Open, "a turn parallel to the wall is physically clear now");
	const auto guidance = EvaluateGuidance(world, left, Scan(world, left, {}, Layer::Terrain), movement, {10, 10}, {});
	Check(guidance.openingSafe, "a current exit is not hidden because continuing straight instead of turning would hit a wall");
}
void BoundaryRules() {
	BoundaryTracker tracker;
	Probe p; p.end = State::Blocked; p.clearDepth = 10; p.hit = {120, 100};
	const auto tick = [&](uint64_t now) { return tracker.Observe(p, {1, 0}, now, PollMs, 20); };
	Check(tick(1000) == State::Unknown, "the first world boundary establishes a silent baseline");
	p.clearDepth = 9;
	Check(tick(1050) == State::Unknown, "approaching the same world hit is not a new geometric edge");
	p.hit.y += 10;
	Check(tick(1100) == State::Unknown, "parallel movement along a wall does not retrigger its cue");
	p.hit.x += 1;
	Check(tick(1150) == State::Unknown, "a one-subtile staircase does not produce an opening");
	p.hit.x += 5;
	Check(tick(1200) == State::Unknown, "a boundary jump waits for confirmation");
	p.hit.x -= 5;
	Check(tick(1250) == State::Unknown, "a one-poll gap is not announced");
	p.end = State::Unknown; p.clearDepth = 5;
	Check(tick(1300) == State::Unknown, "losing local terrain coverage is not an opening");
	p.end = State::Blocked;
	Check(tick(1350) == State::Unknown, "restored coverage rebases without an invented closure");
	p.end = State::Unknown; p.clearDepth = 30;
	(void)tick(1400);
	Check(tick(1450) == State::Open, "inspected local range can open without requiring distant room residency");
	p.end = State::Blocked; p.clearDepth = 50;
	Check(tick(1500) == State::Unknown, "a far wall beyond the reference range cannot chatter");
	p.clearDepth = 10;
	(void)tick(1550);
	Check(tick(1600) == State::Blocked, "a wall returning inside the scanned range announces its arrival");
}
void OccupiedRecess() {
	auto world = Field(); Wall(world, 216, 1); Wall(world, 226, 1);
	Engine engine; (void)engine.Tick(world, 1000);
	Wall(world, 216, 0);
	world.rooms[0].cells[101*240+101] = 0x1100;
	(void)engine.Tick(world, 1050);
	Check(engine.Tick(world, 1100)[3] == State::Unknown, "a new recessed wall cannot announce a passage occupied by a creature");
	world.rooms[0].cells[101*240+101] = 0;
	Check(engine.Tick(world, 1150)[3] == State::Open, "a confirmed recessed boundary can sound once the current body passage clears");
	AudioLoad failure; failure.failed[3] = {State::Open, 1150};
	Check(engine.Tick(world, 1200, failure)[3] == State::Unknown, "failed geometry cues respect the same retry spacing");
	(void)engine.Tick(world, 1300);
	Check(engine.Tick(world, 1400)[3] == State::Open, "a still-valid recessed-wall cue recovers from failed playback");
	Wall(world, 216, 1); (void)engine.Tick(world, 1450);
	Check(engine.Tick(world, 1500)[3] == State::Blocked, "a returned finite boundary closes the previous recess");
	for (uint64_t now = 1550; now <= 1800; now += 50) {
		Check(engine.Tick(world, now)[3] == State::Unknown && engine.Sensors()[3].cueState == State::Blocked,
			"a geometry closure remains valid for queued playback without reopening itself");
	}
	Check(engine.Tick(world, 1850, failure)[3] == State::Unknown, "an obsolete opening failure cannot resurrect a closed recess");
}
void RecordedWallSlide() {
	// Ctrl+T 2026-09-15 23:52:33: all 25 cells of rows -12..-2 blocked,
	// all rows -1..12 clear; native pattern 1. The grid is in world axes.
	auto world = Field(); world.player = {4809, 4548}; world.subtileFraction = {0.67870, 0.58472};
	world.rooms = {{4797, 4536, 25, 25, std::vector<uint16_t>(25*25)}};
	for (int row = 0; row < 11; ++row) for (int x = 0; x < 25; ++x) world.rooms[0].cells[row*25+x] = 0x0005;
	Engine engine; (void)engine.Tick(world, 1000);
	Check(D2RAccess::RadarReadout::Describe(engine, D2RAccess::Strings::Spanish).find("todavía sin medir") != std::string::npos,
		"a single sample does not falsely announce that the player is stationary");
	world.subtileFraction.x += 0.16; (void)engine.Tick(world, 1050);
	Check(engine.Sensors()[0].probe.observed == State::Blocked && engine.Sensors()[2].probe.observed == State::Blocked,
		"the captured wall blocks straight up and right");
	Check(Scan(world, {1, 0}, {}).observed == State::Open, "the captured map still allows movement along world X, diagonally on screen");
	const auto text = D2RAccess::RadarReadout::Describe(engine, D2RAccess::Strings::Spanish);
	Check(text.find("Desplazándote hacia abajo y derecha") == 0 && text.find("Pasos en línea recta") != std::string::npos,
		"Ctrl+T distinguishes actual diagonal movement from blocked straight rays");
	(void)engine.Tick(world, 1150); (void)engine.Tick(world, 1250);
	Check(D2RAccess::RadarReadout::Describe(engine, D2RAccess::Strings::Spanish).find("Sin desplazamiento reciente") == 0,
		"Ctrl+T reports an actual stop after the motion history settles");
	++world.session; (void)engine.Tick(world, 1300);
	Check(!engine.HasMotionSamples(), "lifecycle resets also discard the spoken movement baseline");
	constexpr std::array<Position, 8> directions{{{-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}}};
	for (size_t i=0; i<directions.size(); ++i)
		Check(D2RAccess::RadarReadout::MovementDirection(directions[i]) == static_cast<int>(i), "native projection maps movement into the correct screen sector");
	Check(D2RAccess::RadarReadout::MovementDirection({}) == -1, "zero displacement has no invented direction");
}
}
int main() { FiniteOpening(); TurnBeforeWall(); BoundaryRules(); OccupiedRecess(); RecordedWallSlide(); Passed(); }
