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
Events Engine::Tick(const World& world, uint64_t now, AudioLoad audio) {
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
	if (rebase) audio = {}; // the service cancels old-world audio after this tick
	Events candidates{}, all{};
	all.fill(State::Open); // reservation mask, not a statement about clear passages
	for (size_t i = 0; i < sensors_.size(); ++i) {
		auto& sensor = sensors_[i];
		sensor.probe = Scan(world, Axes[i], settings_);
		sensor.terrain = Scan(world, Axes[i], settings_, Layer::Terrain);
		const auto change = sensor.boundary.Observe(sensor.terrain, Axes[i], now, settings_.stableMs, settings_.boundaryRange);
		if (change != State::Unknown) {
			sensor.boundaryPending = change;
			sensor.pendingBoundary = sensor.boundary.Current();
		}
		if (!rebase && sensor.boundaryPending != State::Unknown) candidates[i] = sensor.boundaryPending;
		// A longer queue can advance a warning or remove an opening. Reserve
		// either possible transition before deciding this poll's final budget.
		for (const uint64_t budget : {PlaybackBudget(audio, {}, i), PlaybackBudget(audio, all, i)}) {
			const auto possible = ClassifyGuidance(Axes[i], sensor.terrain, velocity_, settings_, budget).observed;
			if (!rebase && possible != State::Unknown && possible != sensor.stable) candidates[i] = possible;
		}
	}
	for (size_t i = 0; i < sensors_.size(); ++i) {
		auto& sensor = sensors_[i];
		sensor.guidance = EvaluateGuidance(world, Axes[i], sensor.terrain, sensor.probe, velocity_, settings_, PlaybackBudget(audio, candidates, i));
		const State observed = sensor.guidance.observed;
		sensor.cueState = observed == State::Open && !sensor.guidance.openingSafe ? State::Unknown : observed;
		// Keep a confirmed wall through the limited-clearance band. A change
		// in speed/queue length alone must not truncate its warning sound.
		if (observed == State::Unknown && sensor.stable == State::Blocked
			&& sensor.terrain.available && sensor.terrain.end == State::Blocked) sensor.cueState = State::Blocked;
		if (!sensor.terrain.available) {
			sensor = Sensor{.probe = sensor.probe};
			continue;
		}
		if (rebase) {
			sensor.stable = observed;
			sensor.hasBoundary = sensor.terrain.end == State::Blocked;
			continue;
		}
		const auto failure = audio.failed[i];
		if (sensor.lastEmit && failure.eventMs == sensor.lastEmit && failure.state == sensor.lastCue) sensor.retry = true;
		if (observed == State::Unknown || (observed == sensor.stable && (!sensor.retry || sensor.boundaryCue))
			|| (observed == State::Open && !sensor.guidance.openingSafe)) {
			sensor.candidate = State::Unknown;
			sensor.candidateTravel = 0;
		} else {
			if (observed != sensor.candidate) {
				sensor.candidate = observed;
				sensor.candidateSince = now;
				sensor.candidateTravel = travel;
			} else sensor.candidateTravel += travel;
			if (now - sensor.candidateSince >= settings_.stableMs
				&& (observed != sensor.stable || now - sensor.lastEmit >= settings_.retryMs)) {
				if (sensor.stable != State::Unknown || sensor.hasBoundary) events[i] = observed;
				if (observed == State::Blocked) sensor.hasBoundary = true;
				sensor.stable = observed;
				sensor.candidate = State::Unknown;
			}
		}
		if (!sensor.boundary.Accepts(sensor.pendingBoundary, sensor.boundaryPending)) sensor.boundaryPending = State::Unknown;
		if (events[i] != State::Unknown) {
			// One cue when a geometry change and a clearance change coincide.
			sensor.boundaryCue = false;
			sensor.boundaryPending = State::Unknown;
		} else if (sensor.boundaryPending != State::Unknown
			&& (sensor.boundaryPending == State::Blocked || sensor.guidance.openingSafe)) {
			events[i] = sensor.boundaryPending;
			sensor.boundaryCue = true;
			sensor.emittedBoundary = sensor.pendingBoundary;
			sensor.boundaryPending = State::Unknown;
			if (observed != State::Unknown) sensor.stable = observed;
			sensor.candidate = State::Unknown;
		}
		const State lastCue = events[i] != State::Unknown ? events[i] : sensor.lastCue;
		if (sensor.boundaryCue) {
			sensor.cueState = sensor.boundary.Accepts(sensor.emittedBoundary, lastCue)
				&& (lastCue == State::Blocked || sensor.guidance.openingSafe) ? lastCue : State::Unknown;
			if (events[i] == State::Unknown && sensor.retry && sensor.cueState == lastCue
				&& now - sensor.lastEmit >= settings_.retryMs) events[i] = lastCue;
		}
		if (events[i] != State::Unknown) {
			sensor.lastEmit = now;
			sensor.lastCue = events[i];
			sensor.retry = false;
		}
	}
	return events;
}
}
