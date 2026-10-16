#include "radar.h"
#include "radar_passage.h"
#include "rooms.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace D2RAccess::Radar {
namespace {
double Distance(Position a, Position b) { return std::hypot(a.x-b.x, a.y-b.y); }
// A body stopped at a footprint position meets small clutter when every
// blocked cell under it is clutter.
bool SmallAhead(const World& structure, int x, int y) {
	const int reach = structure.bodyPattern == 0 ? 0 : 1;
	bool blocked = false;
	for (int dy = -reach; dy <= reach; ++dy) for (int dx = -reach; dx <= reach; ++dx) {
		if (structure.Cell(x + dx, y + dy) != State::Blocked) continue;
		if (!SmallObstacle(structure, x + dx, y + dy)) return false;
		blocked = true;
	}
	return blocked;
}
}
void Engine::Reset() {
	const uint64_t nextRevision = revision_ + 1, wallRepeat = wallRepeatMs_; // a setting, not an observation
	*this = Engine(settings_);
	revision_ = nextRevision;
	wallRepeatMs_ = wallRepeat;
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
void Engine::TrackRoom(const World& world, Position motion, Position travel) {
	// The mark is worth one reading near the doorway, not a memory of it.
	for (auto& sensor : sensors_)
		if (sensor.wayBack && Distance(motion, sensor.wayBackAt) > wayThrough_) sensor.wayBack = false;
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
		const auto& axis = RaysOf(settings_.frame)[i].world;
		const double along = travel.x*axis.x + travel.y*axis.y;
		if (along < best) { best = along; back = i; }
	}
	if (best < 0) { sensors_[back].wayBack = true; sensors_[back].wayBackAt = motion; }
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
		sensor.small = passage.small;
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
			sensor.small = passage.small;
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
		// Dropped, not held: D2 walls meet the cardinal rays at 45 degrees, so a
		// held opening would sound as soon as the player walked along the same wall.
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
		sensor.lastCueUnexplored = sensor.deferredUnexplored;
		sensor.lastCueMs = sensor.lastOpeningMs = now;
		sensor.deferred = State::Unknown;
		sensor.hold = "way-back";
		return State::Unknown;
	}
	if (Passable(sensor.deferred) && sensor.deferred == sensor.lastCue && sensor.deferredUnexplored == sensor.lastCueUnexplored) {
		// Back to what the ear was last told before the change could sound.
		sensor.deferred = State::Unknown;
		sensor.hold = "unchanged";
		return State::Unknown;
	}
	// Counted from the last opening, not from a wall: a gap passed right after
	// its wall was announced would otherwise wait until the wall returned.
	if (Passable(sensor.deferred) && sensor.lastOpeningMs && now - sensor.lastOpeningMs < settings_.cueIntervalMs) {
		sensor.hold = "interval";
		return State::Unknown;
	}
	sensor.lastCue = sensor.deferred;
	sensor.lastCueUnexplored = sensor.deferredUnexplored;
	sensor.lastCueMs = now;
	if (Passable(sensor.lastCue)) sensor.lastOpeningMs = now;
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
void Engine::RepeatWall(Events& events, Position heading, uint64_t now, Marks* small) {
	if (!(heading.x || heading.y)) heading = velocity_;
	const double length = std::hypot(heading.x, heading.y);
	if (!wallRepeatMs_ || !(length > 0)) return;
	// The ray within 45 degrees of the heading; none on the exact diagonal between two.
	const auto& rays = RaysOf(settings_.frame);
	size_t ahead = sensors_.size();
	double best = std::numbers::sqrt2 / 2;
	for (size_t i = 0; i < rays.size(); ++i) {
		const double along = (heading.x * rays[i].world.x + heading.y * rays[i].world.y)
			/ (length * std::hypot(rays[i].world.x, rays[i].world.y));
		if (along > best) { best = along; ahead = i; }
	}
	if (ahead == sensors_.size() || events[ahead] != State::Unknown) return;
	auto& sensor = sensors_[ahead];
	if (!Obstacle(sensor.state) || !sensor.passage.available || !Obstacle(sensor.passage.observed)
		|| now - sensor.lastCueMs < wallRepeatMs_) return;
	sensor.lastCueMs = now;
	events[ahead] = sensor.state;
	if (small && sensor.state == State::Blocked) (*small)[ahead] = sensor.small;
}
Events Engine::Tick(const World& world, uint64_t now, Marks* marks, Marks* small, Position heading) {
	Events events{};
	if (marks) *marks = {};
	if (small) *small = {};
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
	if (rebase || world.rooms.size() != wayThroughRooms_) {
		wayThrough_ = WayThrough(world, settings_);
		wayThroughRooms_ = world.rooms.size();
	}
	TrackRoom(world, motion, step);
	const double close = CloseDepth(settings_, world.bodyPattern);
	const double open = settings_.openDepth;
	// The passage window stays inside the structural window it reads from.
	const double span = BodySpan(world.bodyPattern);
	const double reach = std::min(std::max(open, settings_.reachDepth), StructuralRadius(settings_, world.bodyPattern) - 1 - span);
	const auto structure = StructuralWorld(world, settings_);
	auto grid = Navigation::Build(structure, PassageRadius(reach, world.bodyPattern));
	Navigation::LabelRooms(grid, world); // the structural snapshot is one room; membership comes from the real one
	PassageQuery query{.openDepth = open, .closeDepth = close, .maze = world.maze, .wayThrough = wayThrough_, .exits = &world.exits};
	const auto& rays = RaysOf(settings_.frame);
	for (size_t i = 0; i < sensors_.size(); ++i) {
		query.committed = sensors_[i].state;
		auto& passage = sensors_[i].passage;
		passage = ScanPassage(grid, rays[i].world, query);
		passage.unexplored = Qualify(passage, world, i);
		passage.small = world.smallObstacles && passage.available && passage.observed == State::Blocked
			&& passage.obstacle && SmallAhead(structure, passage.obstacleX, passage.obstacleY);
		events[i] = Observe(sensors_[i], rays[i].world, motion, now);
		if (marks && events[i] != State::Unknown) (*marks)[i] = sensors_[i].lastCueUnexplored;
		if (small && events[i] == State::Blocked) (*small)[i] = sensors_[i].small;
	}
	RepeatWall(events, heading, now, small);
	return events;
}
}
