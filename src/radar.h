#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

namespace D2RAccess::Radar {
struct Position { double x = 0, y = 0; };
enum class State { Unknown, Open, Blocked };
enum class Direction { Up, Left, Right, Down }; // fixed screen / left-stick axes
inline constexpr size_t DirectionCount = 4;
using Events = std::array<State, DirectionCount>; // Unknown means no event.
// Shared audio timing: guidance includes a whole four-cue batch in its forecast.
inline constexpr uint64_t CueDurationMs = 120, CueGapMs = 50;
enum class Layer { Movement, Terrain };

// Tuning defaults from D2R_SONAR_RADAR_IMPLEMENTATION_SPEC.md, not game offsets.
struct Settings {
	double farRange = 150; // 30 map tiles; D2 collision has five subtiles per tile.
	double nearRange = 2; // subtile body sweep; avoid rejecting a narrow side doorway.
	double openDepth = 8;
	double blockedDepth = 3;
	double laneSpacing = 1;
	double minTravel = 0.5;
	double teleportDistance = 20; // discontinuity between 50 ms observations
	uint64_t stableMs = 100, cooldownMs = 250, maxGapMs = 250;
	uint64_t stationaryMs = 250; // confirm a closed/open door even when movement stops
	uint64_t reactionMs = 600; // design allowance after hearing a cue, not a game constant
	uint64_t motionWindowMs = 250, stoppedMs = 200; // smooth motion without lingering at rest
	uint64_t recentMotionMs = 100; // detect acceleration before a candidate can be confirmed
	double boundaryChange = 3; // one full cross-footprint width; ignore sub-cell hit-point jitter
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
struct Probe {
	std::array<double, 3> depths{};
	double bodyDepth = 0, medianDepth = 0;
	double clearDepth = 0; // continuous clearance for the complete body, not a point ray
	State body = State::Unknown;
	State end = State::Unknown;
	State observed = State::Unknown;
	bool available = false;
	uint16_t endFlags = 0; // raw flags at the first body obstruction, for diagnostics
};
struct Guidance {
	State observed = State::Unknown;
	bool openingSafe = false;
	double openDepth = 0, warningDepth = 0;
};
struct Sensor {
	State stable = State::Unknown, candidate = State::Unknown;
	uint64_t candidateSince = 0, lastEmit = 0;
	double candidateTravel = 0;
	Probe probe;
	Probe terrain;
	Guidance guidance;
	State cueState = State::Unknown; // currently valid sound, separate from manual physical clearance
	double blockedBoundary = 0, warningBoundary = 0;
	Position warningPosition{};
	bool hasBoundary = false, warned = false;
};

class Engine {
public:
	explicit Engine(Settings settings = {}) : settings_(settings) {}
	Events Tick(const World& world, uint64_t now);
	void Reset();
	const auto& Sensors() const { return sensors_; }
	uint64_t Revision() const { return revision_; } // cancel pending audio after a rebase
	Position Velocity() const { return velocity_; }
private:
	void UpdateMotion(Position player, uint64_t now);
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
Probe SweepBody(const World& world, Position origin, Position axis, double range, double nearRange, Layer layer);
Probe Scan(const World& world, Position axis, const Settings& settings, Layer layer = Layer::Movement);
Guidance EvaluateGuidance(const World& world, Position axis, const Probe& terrain, const Probe& movement,
	Position velocity, const Settings& settings);
}
