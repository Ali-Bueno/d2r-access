#include "radar_readout.h"
#include "strings/enUS.h"
#include "test_support.h"

#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>

using namespace D2RAccess;
using namespace Radar;
namespace {
constexpr int Size = 240;
constexpr uint16_t PlayerPath = 0x1C09, Obstacle = 1, Blank = 0x20;
constexpr size_t Index(Direction direction) { return static_cast<size_t>(direction); }
const auto& Text = Strings::Tables::enUS; // header-only: this target does not link speech_strings.cpp

World Field() {
	World world;
	world.session = world.level = 1;
	world.player = {110, 100}; // world x+y = 210, world x-y = 10
	world.bodyPattern = 1;
	world.walkingMask = PlayerPath;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size)});
	return world;
}
void Put(World& world, int x, int y, uint16_t flags) {
	world.rooms[0].cells[static_cast<size_t>(y) * Size + x] = flags;
}
// Barriers along constant x+y face the screen up/down rays head-on; barriers
// along constant x-y face the screen left/right rays.
void Sum(World& world, int sum, uint16_t flags) {
	for (int y = 0; y < Size; ++y) { const int x = sum - y; if (x >= 0 && x < Size) Put(world, x, y, flags); }
}
void Difference(World& world, int difference, uint16_t flags) {
	for (int y = 0; y < Size; ++y) { const int x = difference + y; if (x >= 0 && x < Size) Put(world, x, y, flags); }
}
std::string Depth(double cells) {
	char text[80]{};
	std::snprintf(text, sizeof(text), Text.radarDepth, cells);
	return text;
}
std::string Entry(Direction direction, const std::string& body) {
	return std::string(Text.radarDirections[Index(direction)]) + ": " + body;
}
bool Contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

void Passages() {
	// Each barrier is placed so that the proved depth lands in a different
	// band: none at all, inside the closing depth, inside the dead band
	// between the thresholds, and beyond the opening depth.
	const double close = Engine::CloseDepth({}, 1), open = Settings{}.openDepth;
	const double step = std::numbers::sqrt2 / 2; // one lattice step along a world diagonal
	auto world = Field();
	Sum(world, 212, Obstacle);      // two cells ahead of the down ray: no route at all
	Sum(world, 202, Obstacle);      // six cells behind: a short route up
	Difference(world, -4, Obstacle); // twelve cells to the left: inside the dead band
	Engine engine;
	(void)engine.Tick(world, 1000);
	const auto& sensors = engine.Sensors();
	Check(sensors[Index(Direction::Down)].passage.depth == 0
		&& std::abs(sensors[Index(Direction::Up)].passage.depth - 6 * step) < 1e-9
		&& std::abs(sensors[Index(Direction::Left)].passage.depth - 12 * step) < 1e-9
		&& sensors[Index(Direction::Right)].passage.depth >= open,
		"the four barriers produce one depth in each band the readout has to describe");
	const auto readout = RadarReadout::Describe(engine, Text);
	Check(readout.starts_with(Text.radarMotionUnknown), "a single sample cannot claim the player is standing still");
	Check(Contains(readout, Text.radarStraight), "the passage list is introduced as straight-line travel");
	size_t previous = 0;
	for (size_t i = 0; i < DirectionCount; ++i) {
		const size_t at = readout.find(std::string(Text.radarDirections[i]) + ": ");
		Check(at != std::string::npos && at > previous, "the four directions are named once, in the engine's own order");
		previous = at;
	}
	Check(Contains(readout, Entry(Direction::Down, std::string(Text.radarStates[2]) + ", " + Text.radarNearbyObstacle)),
		"a wall with no route at all in front of it is reported as an obstacle nearby");
	Check(Contains(readout, Entry(Direction::Up, std::string(Text.radarStates[2]) + ", " + Depth(std::floor(6 * step)))),
		"a blocked direction reports the whole cells of route it still has");
	Check(Contains(readout, Entry(Direction::Left, std::string(Text.radarLimited) + ", " + Depth(std::floor(12 * step)))),
		"a depth between the two thresholds is described as limited space, not as open or blocked");
	Check(Contains(readout, Entry(Direction::Right, std::string(Text.radarStates[1]) + ", ")),
		"a proved passage is reported as open");
	Check(close < 12 * step && 12 * step < open, "the limited case really sits inside the engine's dead band");
}

void DiagonalNames() {
	// On the diagonal frame each ray is named by its screen sector, in the
	// engine's order, and the switch speaks the same four names.
	auto world = Field();
	Engine engine({.frame = Frame::Diagonal});
	(void)engine.Tick(world, 1000);
	const auto readout = RadarReadout::Describe(engine, Text);
	const auto list = RadarReadout::DirectionList(Frame::Diagonal, Text);
	size_t previous = 0, listed = 0;
	for (const auto& ray : RaysOf(Frame::Diagonal)) {
		const std::string name = Text.radarMovementDirections[static_cast<size_t>(ray.sector)];
		const size_t at = readout.find(name + ": ");
		Check(at != std::string::npos && at > previous, "each diagonal is named after its screen sector, in the engine's order");
		previous = at;
		const size_t inList = list.find(name, listed);
		Check(inList != std::string::npos && inList >= listed, "the spoken list names the four diagonals in the same order");
		listed = inList + name.size();
	}
	Check(RadarReadout::DirectionList(Frame::Cardinal, Text).starts_with(Text.radarDirections[0]),
		"the cardinal list keeps the radar's own direction words");
}

void UnknownCoverage() {
	auto world = Field();
	Sum(world, 212, Blank);
	Engine engine;
	(void)engine.Tick(world, 1000);
	const auto readout = RadarReadout::Describe(engine, Text);
	Check(!engine.Sensors()[Index(Direction::Down)].passage.available, "the unmapped cells really withdrew the verdict");
	Check(Contains(readout, Entry(Direction::Down, Text.radarStates[0])),
		"a direction without coverage is reported as unknown");
	Check(!Contains(readout, Entry(Direction::Down, std::string(Text.radarStates[0]) + ", ")),
		"an unknown direction never quotes a depth it could not measure");
}

void Motion() {
	auto world = Field();
	Engine engine;
	(void)engine.Tick(world, 1000);
	(void)engine.Tick(world, 1050);
	Check(RadarReadout::Describe(engine, Text).starts_with(Text.radarStationary),
		"a second sample at the same cell reports that the player is not moving");
	world.player = {111, 101}; // world down-right and down-left: screen down
	(void)engine.Tick(world, 1100);
	char moving[160]{};
	std::snprintf(moving, sizeof(moving), Text.radarMoving, Text.radarMovementDirections[4]);
	Check(RadarReadout::Describe(engine, Text).starts_with(moving),
		"actual movement is described by the screen sector the player is crossing");
}

void MovementSectors() {
	// The eight screen sectors, clockwise from up, as world velocities.
	constexpr std::array<Position, 8> world{{{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}}};
	for (size_t sector = 0; sector < world.size(); ++sector) {
		Check(RadarReadout::MovementDirection(world[sector]) == static_cast<int>(sector),
			"the native projection maps each world heading into its own screen sector");
		Check(RadarReadout::MovementDirection({world[sector].x * 37, world[sector].y * 37}) == static_cast<int>(sector),
			"the sector depends on the heading only, never on how fast the player is going");
	}
	Check(RadarReadout::MovementDirection({}) == -1, "standing still has no invented direction");
	// The radar's own four axes land on the screen sectors they are named for.
	for (size_t i = 0; i < DirectionCount; ++i) {
		const auto screen = ScreenAxes[i];
		const auto expected = std::lround(std::atan2(static_cast<double>(screen.x), -static_cast<double>(screen.y))
			/ (std::numbers::pi / 4));
		Check(RadarReadout::MovementDirection(Axes[i]) == static_cast<int>((expected + 8) % 8),
			"walking along a radar axis is described as movement towards that same screen side");
	}
}
}
int main() {
	Passages();
	DiagonalNames();
	UnknownCoverage();
	Motion();
	MovementSectors();
	Passed();
}
