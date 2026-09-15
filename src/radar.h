#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace D2RAccess::Radar {
struct Position { double x = 0, y = 0; };
enum class State { Unknown, Open, Blocked };
enum class Direction { Up, Left, Right, Down }; // fixed screen / left-stick axes
inline constexpr size_t DirectionCount = 4;
using Events = std::array<State, DirectionCount>; // Unknown means no event.

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
};

struct RoomGrid {
	int x = 0, y = 0, width = 0, height = 0;
	std::vector<uint16_t> cells;
};
struct World {
	Position player;
	uint64_t session = 0;
	int level = 0, bodyPattern = 0;
	uint16_t walkingMask = 0;
	std::vector<RoomGrid> rooms;
	State Cell(int x, int y) const;
	State Body(int x, int y) const;
};
struct Probe {
	std::array<double, 3> depths{};
	double bodyDepth = 0, medianDepth = 0;
	double clearDepth = 0; // continuous clearance for the complete body, not a point ray
	State body = State::Unknown;
	State end = State::Unknown;
	State observed = State::Unknown;
	bool available = false;
};
struct Sensor {
	State stable = State::Unknown, candidate = State::Unknown;
	uint64_t candidateSince = 0, lastEmit = 0;
	double candidateTravel = 0;
	Probe probe;
};

class Engine {
public:
	explicit Engine(Settings settings = {}) : settings_(settings) {}
	Events Tick(const World& world, uint64_t now);
	void Reset();
	const auto& Sensors() const { return sensors_; }
	uint64_t Revision() const { return revision_; } // cancel pending audio after a rebase
private:
	Settings settings_;
	std::array<Sensor, DirectionCount> sensors_{};
	Position previous_{};
	uint64_t session_ = 0, lastTick_ = 0;
	int level_ = 0;
	bool positioned_ = false;
	uint64_t revision_ = 0;
};
Probe Scan(const World& world, Position axis, const Settings& settings);
}
