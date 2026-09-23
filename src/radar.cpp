#include "radar.h"
#include "radar_passage.h"
#include "rooms.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
namespace {
double Distance(Position a, Position b) { return std::hypot(a.x-b.x, a.y-b.y); }
}
void Engine::Reset() {
	const uint64_t nextRevision = revision_ + 1;
	*this = Engine(settings_);
	revision_ = nextRevision;
}
double Engine::Speed() const { return std::hypot(velocity_.x, velocity_.y); }
double Engine::CloseDepth(const Settings& settings, int bodyPattern) {
	return std::max(settings.blockedDepth, settings.openDepth - BodySpan(bodyPattern));
}
double Engine::WayThrough(const World& world, const Settings& settings) {
	// Wider than the opening depth would ask for more proof than the sensor
	// itself calls an opening; narrower than the body could not be walked.
	const int typical = world.maze ? Rooms::TypicalOpeningWidth(world) : 0;
	if (typical <= 0) return settings.openDepth;
	return std::clamp(static_cast<double>(typical), BodySpan(world.bodyPattern), settings.openDepth);
}
void Engine::UpdateMotion(Position player, uint64_t now) {
	if (!motion_.empty() && (player.x != motion_.back().position.x || player.y != motion_.back().position.y)) lastMove_ = now;
	if (!motion_.empty() && motion_.back().time == now) motion_.back().position = player;
	else motion_.push_back({player, now});
	while (motion_.size() > 1 && now - motion_.front().time > settings_.motionWindowMs) motion_.pop_front();
	velocity_ = {};
	const auto& first = motion_.front();
	if (now == first.time || now - lastMove_ >= settings_.stoppedMs) return;
	const double seconds = (now - first.time) / 1000.0;
	velocity_ = {(player.x - first.position.x) / seconds, (player.y - first.position.y) / seconds};
	// Use recent motion to describe acceleration and turns without lingering
	// on the previous direction. Motion never generates geometry cues.
	const auto recent = std::find_if(motion_.begin(), motion_.end(), [&](const auto& sample) { return now - sample.time <= settings_.recentMotionMs; });
	if (recent == motion_.end() || recent->time == now) return;
	const double recentSeconds = (now - recent->time) / 1000.0;
	const Position delta{player.x - recent->position.x, player.y - recent->position.y};
	const double distance = std::hypot(delta.x, delta.y);
	if (distance == 0) return;
	const double speed = std::max(Speed(), distance / recentSeconds);
	velocity_ = {delta.x * speed / distance, delta.y * speed / distance};
}
void Engine::TrackRoom(const World& world, Position travel) {
	const int index = world.RoomAt(static_cast<int>(world.player.x), static_cast<int>(world.player.y));
	if (index < 0) return; // off-grid is not a room change
	const auto& room = world.rooms[static_cast<size_t>(index)];
	const bool changed = roomKnown_ && (room.x != roomX_ || room.y != roomY_);
	roomX_ = room.x; roomY_ = room.y; roomKnown_ = true;
	if (!changed || !world.maze) return;
	// The doorway just crossed lies behind the player: the direction most
	// against the step that crossed it will read it as an exit, and has.
	for (auto& sensor : sensors_) sensor.wayBack = false;
	size_t back = 0;
	double best = 0;
	for (size_t i = 0; i < sensors_.size(); ++i) {
		const double along = travel.x*Axes[i].x + travel.y*Axes[i].y;
		if (along < best) { best = along; back = i; }
	}
	if (best < 0) sensors_[back].wayBack = true;
}
State Engine::Observe(Sensor& sensor, Position axis, Position motion, uint64_t now) const {
	const auto& passage = sensor.passage;
	if (!passage.available) {
		// Coverage loss keeps the confirmed state, stops any confirmation and
		// holds a waiting cue until the passage can be seen again.
		sensor.candidate = State::Unknown;
		return State::Unknown;
	} else if (!sensor.baseline) {
		// Silent baseline: the first observation after a reset never sounds.
		sensor.baseline = true;
		sensor.state = passage.observed;
		sensor.unexplored = passage.unexplored;
	} else if (passage.observed == sensor.state && passage.unexplored == sensor.unexplored) {
		sensor.candidate = State::Unknown;
	} else if (sensor.candidate != passage.observed || sensor.candidateUnexplored != passage.unexplored) {
		sensor.candidate = passage.observed;
		sensor.candidateUnexplored = passage.unexplored;
		sensor.candidateOrigin = motion;
		sensor.candidateSince = now;
	} else {
		const bool travelled = Distance(motion, sensor.candidateOrigin) >= settings_.confirmDistance;
		const bool stationary = now - lastMove_ >= settings_.stoppedMs && now - sensor.candidateSince >= settings_.stationaryConfirmMs;
		if (travelled || stationary) {
			sensor.state = passage.observed;
			sensor.unexplored = passage.unexplored;
			sensor.candidate = State::Unknown;
			sensor.deferred = sensor.state; // replaces any older change still waiting
			sensor.deferredUnexplored = sensor.unexplored;
			sensor.hold = "";
			if (Obstacle(sensor.state))
				sensor.frontier = {true, motion.x*axis.x + motion.y*axis.y + passage.depth, motion};
			if (sensor.state != State::Exit) sensor.wayBack = false;
		}
	}
	return Deliver(sensor, axis, motion, now);
}
State Engine::Deliver(Sensor& sensor, Position axis, Position motion, uint64_t now) const {
	if (sensor.deferred == State::Unknown) return State::Unknown;
	if (Passable(sensor.deferred) && sensor.frontier.known) {
		// Backing away from an announced wall lengthens the route without
		// opening anything. Half the hysteresis band absorbs raster steps;
		// the wall stops gating once the player has left its neighbourhood.
		const auto& frontier = sensor.frontier;
		const Position offset{motion.x - frontier.origin.x, motion.y - frontier.origin.y};
		const double lateral = std::abs(-offset.x*axis.y + offset.y*axis.x);
		const double reach = motion.x*axis.x + motion.y*axis.y + sensor.passage.depth;
		const double margin = (sensor.passage.openDepth - sensor.passage.closeDepth) / 2;
		if (lateral <= settings_.boundaryRange && reach < frontier.projection + margin) {
			sensor.deferred = State::Unknown;
			sensor.hold = "retreat";
			return State::Unknown;
		}
	}
	if (sensor.deferred == State::Exit && sensor.wayBack) {
		// The doorway the player just walked through: known, so taken as read.
		sensor.wayBack = false;
		sensor.lastCue = State::Exit;
		sensor.lastCueMs = now;
		sensor.deferred = State::Unknown;
		sensor.hold = "way-back";
		return State::Unknown;
	}
	if (Passable(sensor.deferred) && sensor.lastCueMs && now - sensor.lastCueMs < settings_.cueIntervalMs) {
		sensor.hold = "interval";
		return State::Unknown;
	}
	sensor.lastCue = sensor.deferred;
	sensor.lastCueUnexplored = sensor.deferredUnexplored;
	sensor.lastCueMs = now;
	sensor.deferred = State::Unknown;
	sensor.hold = "";
	return sensor.lastCue;
}
Marks Engine::Unexplored() const {
	Marks marks{};
	for (size_t i = 0; i < sensors_.size(); ++i) marks[i] = sensors_[i].unexplored;
	return marks;
}
bool Engine::Qualify(const Passage& passage, const World& world, size_t direction) {
	if (!passage.available || !Passable(passage.observed)) return false;
	if (passage.observed == State::Exit) {
		if (passage.exit >= 0 && static_cast<size_t>(passage.exit) < world.exits.size())
			return world.exits[static_cast<size_t>(passage.exit)].unexplored;
		if (passage.crossingRoom >= 0 && static_cast<size_t>(passage.crossingRoom) < world.rooms.size())
			return !world.rooms[static_cast<size_t>(passage.crossingRoom)].visited;
		return false;
	}
	return direction < world.frontier.size() && world.frontier[direction];
}
Events Engine::Tick(const World& world, uint64_t now, Marks* marks) {
	Events events{};
	if (marks) *marks = {};
	// Preserve subcell movement for confirmation and the actual-motion readout.
	// The collision adapter still supplies the native integer footprint origin.
	const Position motion{world.player.x + world.subtileFraction.x, world.player.y + world.subtileFraction.y};
	const double travel = Distance(motion, previous_);
	const bool rebase = !positioned_ || world.session != session_ || world.level != level_
		|| now < lastTick_ || now - lastTick_ > settings_.maxGapMs || travel >= settings_.teleportDistance;
	if (rebase) {
		Reset();
		positioned_ = true;
		session_ = world.session;
		level_ = world.level;
	}
	const Position step = rebase ? Position{} : Position{motion.x - previous_.x, motion.y - previous_.y};
	previous_ = motion;
	lastTick_ = now;
	UpdateMotion(motion, now);
	TrackRoom(world, step);
	if (rebase || world.rooms.size() != wayThroughRooms_) {
		wayThrough_ = WayThrough(world, settings_);
		wayThroughRooms_ = world.rooms.size();
	}
	const double close = CloseDepth(settings_, world.bodyPattern);
	const double open = settings_.openDepth;
	// The passage window stays inside the structural window it reads from.
	const double span = BodySpan(world.bodyPattern);
	const double reach = std::min(std::max(open, settings_.reachDepth), StructuralRadius(settings_, world.bodyPattern) - 1 - span);
	const auto structure = StructuralWorld(world, settings_);
	auto grid = Navigation::Build(structure, PassageRadius(reach, world.bodyPattern));
	Navigation::LabelRooms(grid, world); // the structural snapshot is one room; membership comes from the real one
	PassageQuery query{.openDepth = open, .closeDepth = close, .maze = world.maze, .wayThrough = wayThrough_, .exits = &world.exits};
	for (size_t i = 0; i < sensors_.size(); ++i) {
		query.committed = sensors_[i].state;
		sensors_[i].passage = ScanPassage(grid, Axes[i], query);
		sensors_[i].passage.unexplored = Qualify(sensors_[i].passage, world, i);
		events[i] = Observe(sensors_[i], Axes[i], motion, now);
		if (marks && events[i] != State::Unknown) (*marks)[i] = sensors_[i].lastCueUnexplored;
	}
	return events;
}
}
