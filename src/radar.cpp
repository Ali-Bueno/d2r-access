#include "radar.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace D2RAccess::Radar {
namespace {
// Native projection 0x334DB0: screen X = 80*(X-Y), screen Y = 40*(X+Y).
// Stick up/left/right/down map to these fixed world diagonals.
constexpr double Diagonal = std::numbers::sqrt2 / 2;
constexpr std::array<Position, DirectionCount> Axes{{{-Diagonal, -Diagonal},
	{-Diagonal, Diagonal}, {Diagonal, -Diagonal}, {Diagonal, Diagonal}}};
}
void Engine::Reset() {
	const uint64_t nextRevision = revision_ + 1;
	*this = Engine(settings_);
	revision_ = nextRevision;
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
	// A long averaging window alone underestimates acceleration out of a stop
	// and keeps forecasting the old direction after a turn. Use the recent
	// direction and the larger speed estimate before confirming an opening.
	const auto recent = std::find_if(motion_.begin(), motion_.end(), [&](const auto& sample) { return now - sample.time <= settings_.recentMotionMs; });
	if (recent == motion_.end() || recent->time == now) return;
	const double recentSeconds = (now - recent->time) / 1000.0;
	const Position delta{player.x - recent->position.x, player.y - recent->position.y};
	const double distance = std::hypot(delta.x, delta.y);
	if (distance == 0) return;
	const double speed = std::max(std::hypot(velocity_.x, velocity_.y), distance / recentSeconds);
	velocity_ = {delta.x * speed / distance, delta.y * speed / distance};
}
Events Engine::Tick(const World& world, uint64_t now) {
	Events events{};
	// Preserve subcell movement for speed, turns and confirmation travel. The
	// collision adapter still supplies the native integer footprint origin.
	const Position motion{world.player.x + world.subtileFraction.x, world.player.y + world.subtileFraction.y};
	double travel = std::hypot(motion.x - previous_.x, motion.y - previous_.y);
	const bool rebase = !positioned_ || world.session != session_ || world.level != level_
		|| now < lastTick_ || now - lastTick_ > settings_.maxGapMs || travel >= settings_.teleportDistance;
	if (rebase) {
		Reset();
		positioned_ = true;
		session_ = world.session;
		level_ = world.level;
		travel = 0;
	}
	previous_ = motion;
	lastTick_ = now;
	UpdateMotion(motion, now);
	for (size_t i = 0; i < sensors_.size(); ++i) {
		auto& sensor = sensors_[i];
		sensor.probe = Scan(world, Axes[i], settings_);
		sensor.terrain = Scan(world, Axes[i], settings_, Layer::Terrain);
		sensor.guidance = EvaluateGuidance(world, Axes[i], sensor.terrain, sensor.probe, velocity_, settings_);
		const State observed = sensor.guidance.observed;
		sensor.cueState = observed == State::Open && !sensor.guidance.openingSafe ? State::Unknown : observed;
		const double boundary = world.player.x * Axes[i].x + world.player.y * Axes[i].y + sensor.terrain.clearDepth;
		if (!sensor.terrain.available) {
			sensor = Sensor{.probe = sensor.probe};
			continue;
		}
		if (rebase) {
			sensor.stable = observed;
			sensor.blockedBoundary = boundary;
			sensor.hasBoundary = sensor.terrain.end == State::Blocked;
			continue;
		}
		if (observed == State::Unknown || observed == sensor.stable
			|| (observed == State::Open && !sensor.guidance.openingSafe)) {
			sensor.candidate = State::Unknown;
			sensor.candidateTravel = 0;
			continue;
		}
		if (observed != sensor.candidate) {
			sensor.candidate = observed;
			sensor.candidateSince = now;
			sensor.candidateTravel = travel;
		} else sensor.candidateTravel += travel;
		const uint64_t age = now - sensor.candidateSince;
		if (age < settings_.stableMs) continue;
		// Stopping against an obstacle must not suppress its closing cue.
		// A longer stationary window also confirms doors while rejecting flicker.
		if (sensor.stable != State::Unknown && sensor.candidateTravel < settings_.minTravel && age < settings_.stationaryMs) continue;
		if (sensor.lastEmit && now - sensor.lastEmit < settings_.cooldownMs) continue;
		// Kirandur compares world hit points, not distance from the walking
		// player. Backing away from the same wall is not a newly discovered exit.
		const bool openingChanged = !sensor.hasBoundary || sensor.terrain.end != State::Blocked
			|| boundary - sensor.blockedBoundary >= settings_.boundaryChange;
		const bool newWarning = !sensor.warned || std::abs(boundary - sensor.warningBoundary) >= settings_.boundaryChange
			|| std::hypot(world.player.x - sensor.warningPosition.x, world.player.y - sensor.warningPosition.y) >= settings_.openDepth;
		// A known wall in the limited-distance band is a real baseline too;
		// leaving it can reveal an exit without first approaching within 3 cells.
		if ((sensor.stable != State::Unknown || sensor.hasBoundary) && (observed == State::Open ? openingChanged : newWarning)) {
			events[i] = observed;
			sensor.lastEmit = now;
			sensor.warned = observed == State::Blocked;
			sensor.warningBoundary = boundary;
			sensor.warningPosition = world.player;
		}
		if (observed == State::Blocked) {
			sensor.blockedBoundary = boundary;
			sensor.hasBoundary = true;
		}
		sensor.stable = observed;
		sensor.candidate = State::Unknown;
	}
	return events;
}
}
