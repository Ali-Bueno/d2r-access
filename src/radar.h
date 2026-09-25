#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <numbers>
#include <vector>

namespace D2RAccess::Radar {
struct Position { double x = 0, y = 0; };
// A cell is Open or Blocked. A sensor reading adds what the route means:
// Open is floor that leads nowhere new, Exit a route into another chamber or
// to a level exit, Blocked a wall, Door a closed door. `docs/radar-readings.md`
enum class State { Unknown, Open, Blocked, Exit, Door };
inline constexpr bool Passable(State state) { return state == State::Open || state == State::Exit; }
inline constexpr bool Obstacle(State state) { return state == State::Blocked || state == State::Door; }
// D2 collision bits, corroborated by the native reader (docs/collision-semantics.md).
namespace Collision { inline constexpr uint16_t Wall = 0x0001, Blank = 0x0020, Object = 0x0400, Door = 0x0800; }
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
// Eight screen sectors clockwise from up (radarMovementDirections order) for a
// world offset, through the same projection at its 2:1 ratio; 0 when zero.
inline constexpr int ScreenSectors = 8;
inline int ScreenSector(Position offset) {
	const double x = 2 * (offset.x - offset.y), y = offset.x + offset.y;
	const auto sector = std::lround(std::atan2(x, -y) / (2 * std::numbers::pi / ScreenSectors));
	return static_cast<int>((sector + ScreenSectors) % ScreenSectors);
}
using Events = std::array<State, DirectionCount>; // Unknown means no event.
// The unexplored qualifier of a passable reading: the way leads to ground the
// player has not covered. Rides on the exit/floor cue as a short tail; never a
// state of its own, so walls and doors are untouched. `docs/exploration.md`
using Marks = std::array<bool, DirectionCount>;
inline constexpr uint64_t PollMs = 50;
// One cue is a short tin/tock; the gap keeps successive directions separable.
inline constexpr uint64_t CueDurationMs = 120, CueGapMs = 50;
// Two short pips below the tin, distinct from the beacon's single steady ping.
// Sound-design values chosen for cue separation; docs/exploration.md.
inline constexpr uint64_t UnexploredPipMs = 35, UnexploredPipGapMs = 30, UnexploredMarkGapMs = 30;
inline constexpr uint64_t UnexploredMarkMs = 2 * UnexploredPipMs + UnexploredPipGapMs;
inline constexpr double UnexploredMarkHz = 900;
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
// A level exit as the passage scan sees it; unexplored when the level it leads
// to (0 when unknown) has no room the player has seen yet.
struct LevelExit { Position position; uint32_t level = 0; bool unexplored = false; };
// The room partition of a maze level (Levels.txt DrlgType 1) is one chamber
// per streamed room; elsewhere it cuts across open floor and means nothing.
struct PassageQuery {
	double openDepth = 0, closeDepth = 0;
	State committed = State::Unknown; // the sensor's verdict: inside the hysteresis band it stands
	bool maze = false;
	double wayThrough = 0; // a doorway's width: how far past a border, or how near an exit, a route must reach
	const std::vector<LevelExit>* exits = nullptr; // level exits, in world cells
};

struct RoomGrid {
	int x = 0, y = 0, width = 0, height = 0;
	std::vector<uint16_t> cells;
	bool visited = false; // the player has stood in this room (exploration memory)
};
struct World {
	Position player;
	Position subtileFraction; // native 16.16 low words, for motion only; collision uses player cells
	uint64_t session = 0;
	int level = 0, bodyPattern = 0;
	uint16_t walkingMask = 0;
	std::vector<RoomGrid> rooms;
	bool maze = false; // one streamed room is one chamber (Levels.txt DrlgType 1)
	std::vector<LevelExit> exits; // the level's exits: warps, stairs, portals, border exits
	// Open ground (DrlgType 3): ground never streamed is reachable in that
	// screen quadrant, so floor that way is unexplored. Stamped by the terrain
	// service from the exploration memory; false everywhere else.
	Marks frontier{};
	int RoomAt(int x, int y) const; // index into rooms, or -1
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
	State observed = State::Unknown; // Unknown only while coverage is insufficient
	bool available = false; // false means coverage within openDepth is insufficient, not a dead end
	double crossing = -1; // depth at which the route enters another streamed room, if it does
	int crossingRoom = -1; // index into World::rooms of the room entered at `crossing`
	int exit = -1; // index into the query's level exits when the route arrives at one
	bool door = false; // a closed door stands on the route within the closing depth
	bool unexplored = false; // the qualifier of a passable reading, see Engine::Qualify
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
	bool unexplored = false; // qualifier of `state`; a change of it alone is a change too
	bool baseline = false; // the first observation after a reset is silent, dead band included
	State candidate = State::Unknown;
	bool candidateUnexplored = false;
	Position candidateOrigin; // motion position where the candidate first appeared
	uint64_t candidateSince = 0;
	State deferred = State::Unknown; // confirmed opening waiting for the cue interval
	bool deferredUnexplored = false;
	State lastCue = State::Unknown;
	bool lastCueUnexplored = false;
	uint64_t lastCueMs = 0;
	Frontier frontier;
	bool wayBack = false; // the doorway just crossed: its next exit reading is taken as read
	const char* hold = ""; // why the last confirmed change did not sound (diagnostics)
};

class Engine {
public:
	explicit Engine(Settings settings = {}) : settings_(settings) {}
	// `marks`, when given, receives the unexplored qualifier of each event.
	Events Tick(const World& world, uint64_t now, Marks* marks = nullptr);
	// Confirmed qualifiers, one per direction, for the audio queue's retention.
	Marks Unexplored() const;
	// The qualifier of a passable reading: an exit into a chamber never stood
	// in, a level exit to a level never seen, or open ground towards the
	// frontier of what has been streamed. `docs/exploration.md`
	static bool Qualify(const Passage& passage, const World& world, size_t direction);
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
	// The level's own doorway width, from its streamed rooms; the opening depth
	// when the partition means nothing or no opening is loaded yet.
	static double WayThrough(const World& world, const Settings& settings);
	double CurrentWayThrough() const { return wayThrough_; }
private:
	void TrackRoom(const World& world, Position travel);
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
	double wayThrough_ = 0;
	size_t wayThroughRooms_ = 0; // rooms counted when wayThrough_ was derived
	int roomX_ = 0, roomY_ = 0; // origin of the streamed room the player stands in
	bool roomKnown_ = false;
};
}
