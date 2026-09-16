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
// Shared audio timing. The guard allows one 20 Hz poll for scheduling/driver jitter.
inline constexpr uint64_t CueDurationMs = 120, CueGapMs = 50;
inline constexpr uint64_t PollMs = 50, AudioGuardMs = PollMs;
inline constexpr uint64_t CueSlotMs = CueDurationMs + CueGapMs + AudioGuardMs;
struct CueFailure { State state = State::Unknown; uint64_t eventMs = 0; bool submitted = false; };
struct AudioLoad {
	Events pending{};
	bool busy = false; // playback, its silent gap, or a pending cue the worker may take
	Events playing{};
	std::array<uint64_t, DirectionCount> remainingMs{}; // current queue order, through cue completion
	std::array<CueFailure, DirectionCount> failed{}; // drained once, matched to the generated event
};
uint64_t PlaybackBudget(const AudioLoad& load, const Events& candidates, size_t direction);
enum class Layer { Movement, Terrain };

// Tuning defaults from D2R_SONAR_RADAR_IMPLEMENTATION_SPEC.md, not game offsets.
struct Settings {
	double farRange = 150; // 30 map tiles; D2 collision has five subtiles per tile.
	double boundaryRange = 20; // structural neighborhood; original Kirandur +0x24 range in D2 subtiles
	double nearRange = 2; // subtile body sweep; avoid rejecting a narrow side doorway.
	double openDepth = 8; // useful local travel policy, not the length of a D2 room
	double blockedDepth = 3;
	double laneSpacing = 1;
	double teleportDistance = 20; // discontinuity between 50 ms observations
	uint64_t stableMs = PollMs; // two consecutive observations, including while stationary
	uint64_t retryMs = 250, maxGapMs = 250; // rate-limit failed delivery, never an opposite state change
	uint64_t motionWindowMs = 250, stoppedMs = 200; // smooth motion without lingering at rest
	uint64_t recentMotionMs = 100; // recent direction for the actual-motion readout
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
// A local geometry-only view. Never use it to decide physical passage clearance.
World StructuralWorld(const World& world, const Settings& settings);
int StructuralRadius(const Settings& settings, int bodyPattern);
struct Probe {
	std::array<double, 3> depths{};
	double bodyDepth = 0, medianDepth = 0;
	double clearDepth = 0; // continuous clearance for the complete body, not a point ray
	State body = State::Unknown;
	State end = State::Unknown;
	State observed = State::Unknown;
	bool available = false;
	uint16_t endFlags = 0; // raw flags at the first body obstruction, for diagnostics
	Position hit; // first rejected body centre in collision-grid coordinates
	Position surfaceNormal; // supporting line fitted to neighboring full-body rays; zero if ambiguous
};
struct Boundary {
	State end = State::Unknown;
	double along = 0; // world projection with smooth wall drift removed
	double depth = 0; // current body clearance, for confirming a finite recess/return
};
// Historical ray-boundary model, retained for comparison tests. The runtime
// engine uses PassageTracker below instead of projected hit changes.
class BoundaryTracker {
public:
	State Observe(const Probe& terrain, Position axis, uint64_t now, uint64_t confirmationMs, double range);
	bool Accepts(Boundary target, State change) const;
	Boundary Current() const { return current_; }
	Boundary ChangeOrigin() const { return changeOrigin_; }
	State Candidate() const { return candidate_; }
	uint64_t CandidateSince() const { return since_; }
private:
	Boundary previous_, current_, candidateOrigin_, changeOrigin_;
	double lastHit_ = 0, drift_ = 0;
	Position lastPoint_{}, lastNormal_{};
	State candidate_ = State::Unknown;
	uint64_t since_ = 0;
};
struct Guidance {
	State observed = State::Unknown;
	bool openingSafe = false;
	double openDepth = 0, warningDepth = 0;
	uint64_t playbackMs = 0;
};
struct Passage {
	double depth = 0, closeDepth = 0; // proved forward reach, capped at the first useful-depth destination
	State observed = State::Unknown;
	bool available = false; // false means coverage is insufficient, not a dead end
};
class PassageTracker {
public:
	State Observe(const Passage& passage, bool openingSafe, uint64_t now, uint64_t confirmationMs);
	bool Accepts(State cue, const Passage& passage, bool openingSafe) const;
	State Current() const { return state_; }
	State Candidate() const { return candidate_; }
	uint64_t CandidateSince() const { return since_; }
	State Pending() const { return state_ == reported_ && !(state_ == State::Open && rearmed_) ? State::Unknown : state_; }
	void RearmOpening() { if (announcedOpening_) rearmed_ = true; }
private:
	State state_ = State::Unknown, reported_ = State::Unknown, candidate_ = State::Unknown;
	uint64_t since_ = 0;
	bool announcedOpening_ = false, rearmed_ = false;
};
// A world-space wall reference, independent of the player's current clearance.
// It gates new opening cues; it never overrides physical movement queries.
class WallMemory {
public:
	bool Update(const Probe& wall, Position player, Position axis, bool near,
		uint64_t now, const Settings& settings, const World* structure = nullptr);
	bool Known() const { return known_; }
	Position Point() const { return point_; }
	Position Normal() const { return normal_; }
	double RequiredDepth() const { return requiredDepth_; }
	bool BarrierConfirmed() const { return barrierConfirmed_; }
private:
	void Remember(const Probe& wall, Position axis);
	bool ObserveBarrier(uint64_t now, uint64_t confirmationMs);
	Position point_, normal_;
	bool known_ = false, supported_ = false, clearing_ = false, cleared_ = false;
	uint64_t since_ = 0;
	double requiredDepth_ = 0;
	bool barrierObserved_ = false, barrierConfirmed_ = false;
	uint64_t barrierSince_ = 0;
};
struct Sensor {
	State stable = State::Unknown;
	uint64_t lastEmit = 0;
	Probe probe;
	Probe terrain;
	Probe structure;
	Guidance guidance;
	State cueState = State::Unknown; // currently valid sound, separate from manual physical clearance
	bool retry = false, retrying = false;
	Passage passage, movementPassage;
	PassageTracker passageState;
	WallMemory wallMemory;
	bool newOpening = false;
	State lastCue = State::Unknown;
	State AudibleState() const {
		return cueState == State::Blocked ? State::Blocked : guidance.openingSafe ? State::Open : State::Unknown;
	}
};

class Engine {
public:
	explicit Engine(Settings settings = {}) : settings_(settings) {}
	Events Tick(const World& world, uint64_t now, AudioLoad audio = {});
	void Reset();
	const auto& Sensors() const { return sensors_; }
	uint64_t Revision() const { return revision_; } // cancel pending audio after a rebase
	Position Velocity() const { return velocity_; }
	bool HasMotionSamples() const { return motion_.size() > 1; }
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
	Position velocity, const Settings& settings, uint64_t playbackMs = CueDurationMs + AudioGuardMs);
}
