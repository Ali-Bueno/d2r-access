#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <numbers>
#include <vector>

namespace D2RAccess::Radar {
struct Position { double x = 0, y = 0; };
enum class State { Unknown, Open, Blocked };
// The four left-stick cardinals (fixed screen axes; the user's choice: four
// sounds are enough information). Native projection 0x334DB0: screen
// X = 80*(X-Y), screen Y = 40*(X+Y), so each screen cardinal is a world
// diagonal. D2 walls follow the world axes, which these rays meet at 45
// degrees; the travel-based confirmation absorbs the resulting raster steps.
enum class Direction { Up, Left, Right, Down };
inline constexpr size_t DirectionCount = 4;
inline constexpr double Diagonal = std::numbers::sqrt2 / 2;
inline constexpr std::array<Position, DirectionCount> Axes{{{-Diagonal, -Diagonal},
	{-Diagonal, Diagonal}, {Diagonal, -Diagonal}, {Diagonal, Diagonal}}};
// Screen components of each direction (x right, y down), for stereo position.
struct ScreenVector { int x = 0, y = 0; };
inline constexpr std::array<ScreenVector, DirectionCount> ScreenAxes{{{0, -1}, {-1, 0}, {1, 0}, {0, 1}}};
using Events = std::array<State, DirectionCount>; // Unknown means no event.
inline constexpr uint64_t PollMs = 50;
// One cue is a short tin/tock; the gap keeps successive directions separable.
inline constexpr uint64_t CueDurationMs = 120, CueGapMs = 50;
enum class Layer { Movement, Terrain };

// Navigation policy values (subtile = one D2 collision cell, five per map
// tile). None of them is a game constant; each comment states its origin.
struct Settings {
	double farRange = 150; // native snapshot bound: 30 map tiles
	double boundaryRange = 20; // structural neighbourhood, radar_structure.cpp
	double openDepth = 10; // two map tiles of straight route ahead is an exit; walls close one body span nearer
	double blockedDepth = 3; // lower bound of the closing depth in front of a wall
	double teleportDistance = 20; // discontinuity between two 50 ms observations
	uint64_t maxGapMs = 250; // a longer poll gap discards live evidence
	uint64_t motionWindowMs = 250, stoppedMs = 200, recentMotionMs = 100; // velocity smoothing
	// Confirmation by travel, not time: a changed state must persist while the
	// player moves one native cell. A stationary player confirms after two polls.
	double confirmDistance = 1;
	uint64_t stationaryConfirmMs = 2 * PollMs;
	// At most one opening cue per direction in this interval. A wall cue is a
	// safety warning and is never delayed.
	uint64_t cueIntervalMs = 500;
	// Reach measured beyond the opening depth, so a doorway seen after backing
	// away from a wall can still be proved to lead past that wall.
	double reachDepth = 20;
};

struct RoomGrid {
	int x = 0, y = 0, width = 0, height = 0;
	std::vector<uint16_t> cells;
};
struct World {
	Position player;
	Position subtileFraction; // native 16.16 low words, for motion only; collision uses player cells
	uint64_t session = 0;
	int level = 0, bodyPattern = 0;
	uint16_t walkingMask = 0;
	std::vector<RoomGrid> rooms;
	State Cell(int x, int y, Layer layer = Layer::Movement, uint16_t* flags = nullptr) const;
	State Body(int x, int y, Layer layer = Layer::Movement, uint16_t* flags = nullptr) const;
};
uint16_t TerrainCollisionFlags(uint16_t flags);
// A local geometry-only view: actors removed, bypassable clutter removed.
World StructuralWorld(const World& world, const Settings& settings);
int StructuralRadius(const Settings& settings, int bodyPattern);

struct Passage {
	double depth = 0; // proved connected route along the axis, in cells (up to the scan window)
	double openDepth = 0, closeDepth = 0; // thresholds used for this observation
	State observed = State::Unknown; // Unknown: hysteresis dead band or insufficient coverage
	bool available = false; // false means coverage within openDepth is insufficient, not a dead end
};
// A wall the player has been warned about stays where it is. Backing away
// from it makes the route ahead longer again, but nothing opened: an opening
// cue needs the proved route to reach past this wall's position in the world.
struct Frontier {
	bool known = false;
	double projection = 0; // world position of the wall along the direction axis
	Position origin; // where the player stood when the wall was announced
};
struct Sensor {
	Passage passage;
	State state = State::Unknown; // confirmed verdict, Unknown until the baseline
	bool baseline = false; // the first observation after a reset is silent, dead band included
	State candidate = State::Unknown;
	Position candidateOrigin; // motion position where the candidate first appeared
	uint64_t candidateSince = 0;
	State deferred = State::Unknown; // confirmed opening waiting for the cue interval
	State lastCue = State::Unknown;
	uint64_t lastCueMs = 0;
	Frontier frontier;
	const char* hold = ""; // why the last confirmed change did not sound (diagnostics)
};

class Engine {
public:
	explicit Engine(Settings settings = {}) : settings_(settings) {}
	Events Tick(const World& world, uint64_t now);
	void Reset(); // discard observations; the next known observation is a silent baseline
	const auto& Sensors() const { return sensors_; }
	const Settings& Configuration() const { return settings_; }
	uint64_t Revision() const { return revision_; } // cancel pending audio after a rebase
	Position Velocity() const { return velocity_; }
	double Speed() const;
	bool HasMotionSamples() const { return motion_.size() > 1; }
	// Fixed thresholds: the user asked for the same verdicts whether walking
	// or running. Closing sits one body span under opening (hysteresis band).
	static double CloseDepth(const Settings& settings, int bodyPattern);
private:
	void UpdateMotion(Position player, uint64_t now);
	State Observe(Sensor& sensor, Position axis, Position motion, uint64_t now) const;
	State Deliver(Sensor& sensor, Position axis, Position motion, uint64_t now) const;
	Settings settings_;
	std::array<Sensor, DirectionCount> sensors_{};
	Position previous_{};
	uint64_t session_ = 0, lastTick_ = 0;
	int level_ = 0;
	bool positioned_ = false;
	uint64_t revision_ = 0;
	struct MotionSample { Position position; uint64_t time; };
	std::deque<MotionSample> motion_;
	Position velocity_{};
	uint64_t lastMove_ = 0;
};
}
