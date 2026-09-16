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
	// Use recent motion to describe acceleration and turns without lingering
	// on the previous direction. Motion does not generate geometry cues.
	const auto recent = std::find_if(motion_.begin(), motion_.end(), [&](const auto& sample) { return now - sample.time <= settings_.recentMotionMs; });
	if (recent == motion_.end() || recent->time == now) return;
	const double recentSeconds = (now - recent->time) / 1000.0;
	const Position delta{player.x - recent->position.x, player.y - recent->position.y};
	const double distance = std::hypot(delta.x, delta.y);
	if (distance == 0) return;
	const double speed = std::max(std::hypot(velocity_.x, velocity_.y), distance / recentSeconds);
	velocity_ = {delta.x * speed / distance, delta.y * speed / distance};
}
Events Engine::Tick(const World& world, uint64_t now, AudioLoad audio) {
	Events events{};
	// Preserve subcell movement for the actual-motion readout. The
	// collision adapter still supplies the native integer footprint origin.
	const Position motion{world.player.x + world.subtileFraction.x, world.player.y + world.subtileFraction.y};
	const double travel = std::hypot(motion.x - previous_.x, motion.y - previous_.y);
	const bool rebase = !positioned_ || world.session != session_ || world.level != level_
		|| now < lastTick_ || now - lastTick_ > settings_.maxGapMs || travel >= settings_.teleportDistance;
	if (rebase) {
		Reset();
		positioned_ = true;
		session_ = world.session;
		level_ = world.level;
	}
	previous_ = motion;
	lastTick_ = now;
	UpdateMotion(motion, now);
	if (rebase) audio = {}; // the service cancels old-world audio after this tick
	Events candidates{};
	const auto structure = StructuralWorld(world, settings_);
	for (size_t i = 0; i < sensors_.size(); ++i) {
		auto& sensor = sensors_[i];
		sensor.probe = Scan(world, Axes[i], settings_);
		sensor.terrain = Scan(world, Axes[i], settings_, Layer::Terrain);
		sensor.structure = Scan(structure, Axes[i], settings_, Layer::Terrain);
		// Only real player observations advance confirmation. Collision
		// sliding makes an interpolated path between polls unknowable.
		const auto change = sensor.boundary.Observe(sensor.structure, Axes[i], now, settings_.stableMs, settings_.boundaryRange);
		if (change != State::Unknown) {
			// A niche/occupied recess that never qualified as an opening must
			// not announce an orphan closing when its original wall returns.
			if (sensor.boundaryPending == State::Open && change == State::Blocked
				&& sensor.boundary.Accepts(sensor.pendingOrigin, State::Unknown)) sensor.boundaryPending = State::Unknown;
			else {
				sensor.boundaryPending = change;
				sensor.pendingBoundary = sensor.boundary.Current();
				sensor.pendingOrigin = sensor.boundary.ChangeOrigin();
			}
		}
		candidates[i] = sensor.boundaryPending;
	}
	for (size_t i = 0; i < sensors_.size(); ++i) {
		auto& sensor = sensors_[i];
		sensor.guidance = EvaluateGuidance(world, Axes[i], sensor.terrain, sensor.probe, velocity_, settings_, PlaybackBudget(audio, candidates, i));
		if (!sensor.structure.available || sensor.boundary.Current().end == State::Unknown) {
			sensor = Sensor{.probe = sensor.probe, .terrain = sensor.terrain, .structure = sensor.structure, .guidance = sensor.guidance};
			continue;
		}
		if (sensor.guidance.observed != State::Unknown) sensor.stable = sensor.guidance.observed;
		if (rebase) continue;
		sensor.retrying = false;
		const auto failure = audio.failed[i];
		if (sensor.lastEmit && failure.eventMs == sensor.lastEmit && failure.state == sensor.lastCue) sensor.retry = !failure.submitted;
		if (sensor.boundaryPending != State::Unknown
			&& sensor.boundary.Accepts(sensor.pendingBoundary, sensor.boundaryPending)
			&& (sensor.boundaryPending == State::Blocked || sensor.guidance.openingSafe)) {
			events[i] = sensor.boundaryPending;
			// Consume adjacent changes describing the same opening/corner, as
			// AHC consumes its line after firing. D2's body spans three cells
			// (one for pattern 0); tiny facets within that span are one encounter.
			const double bodySpan = world.bodyPattern == 0 ? 1 : 3;
			if (events[i] == sensor.lastCue
				&& std::max(std::abs(world.player.x-sensor.emittedPosition.x), std::abs(world.player.y-sensor.emittedPosition.y)) < bodySpan
				&& sensor.boundary.Accepts(sensor.emittedBoundary, sensor.lastCue)) events[i] = State::Unknown;
			sensor.emittedBoundary = sensor.pendingBoundary;
			sensor.boundaryPending = State::Unknown;
		}
		const State lastCue = events[i] != State::Unknown ? events[i] : sensor.lastCue;
		sensor.cueState = sensor.boundary.Accepts(sensor.emittedBoundary, lastCue)
			&& (lastCue == State::Blocked || sensor.guidance.openingSafe) ? lastCue : State::Unknown;
		if (events[i] == State::Unknown && sensor.retry && sensor.cueState == lastCue
			&& now - sensor.lastEmit >= settings_.retryMs) {
			events[i] = lastCue;
			sensor.retrying = true;
		}
		if (events[i] != State::Unknown) {
			sensor.lastEmit = now;
			sensor.emittedPosition = world.player;
			sensor.lastCue = events[i];
			sensor.retry = false;
		}
	}
	return events;
}
}
