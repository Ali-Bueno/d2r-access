#include "radar.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
uint64_t PlaybackBudget(const AudioLoad& load, const Events& candidates, size_t direction) {
	uint64_t result = CueDurationMs + AudioGuardMs;
	// Reserve a full in-flight cue/gap if the worker is active. Count every
	// other pending/candidate direction once, regardless of fair-queue order.
	// This also covers the worker taking a pending cue during this poll.
	if (load.busy) result += CueSlotMs;
	for (size_t i = 0; i < DirectionCount; ++i)
		if (i != direction && (load.pending[i] != State::Unknown || candidates[i] != State::Unknown))
			result += CueSlotMs;
	return result;
}
Guidance ClassifyGuidance(Position axis, const Probe& terrain, Position velocity,
	const Settings& settings, uint64_t playbackMs) {
	Guidance result;
	result.playbackMs = playbackMs;
	const double speed = std::hypot(velocity.x, velocity.y);
	const double reaction = settings.reactionMs / 1000.0;
	const double horizon = reaction + playbackMs / 1000.0;
	const double approach = std::max(0.0, velocity.x * axis.x + velocity.y * axis.y);
	result.warningDepth = std::max(settings.blockedDepth, approach * (horizon + settings.stableMs / 1000.0));
	// Open depth covers travel after turning. Round up to the collision sweep's
	// half-cell sampling so a fractional requirement cannot miss its last cell.
	constexpr double Step = 0.5;
	result.openDepth = std::ceil(std::max(settings.openDepth, speed * reaction) / Step) * Step;
	if (!terrain.available) return result;
	if (terrain.end == State::Blocked && terrain.clearDepth <= result.warningDepth) {
		result.observed = State::Blocked;
		return result;
	}
	if (terrain.clearDepth < result.openDepth || terrain.medianDepth < result.openDepth) return result;
	result.observed = State::Open;
	return result;
}
Guidance EvaluateGuidance(const World& world, Position axis, const Probe& terrain, const Probe& movement,
	Position velocity, const Settings& settings, uint64_t playbackMs) {
	auto result = ClassifyGuidance(axis, terrain, velocity, settings, playbackMs);
	if (result.observed != State::Open) return result;
	if (movement.clearDepth < result.openDepth || movement.medianDepth < result.openDepth) return result;
	const double speed = std::hypot(velocity.x, velocity.y);
	const double horizon = (settings.reactionMs + playbackMs) / 1000.0;
	const double advance = speed * horizon;
	// Forecast only within our acquired local range. Missing coverage rejects a
	// promise of an opening; it never invents an obstacle or an exit.
	if (advance > settings.farRange || result.openDepth > settings.farRange) return result;
	if (speed > 0) {
		constexpr double Step = 0.5; // same half-cell sampling as the body sweep
		const Position heading{velocity.x / speed, velocity.y / speed};
		for (double distance = Step; distance < advance + Step; distance += Step) {
			const double travel = std::min(distance, advance);
			const Position origin{world.player.x + heading.x * travel, world.player.y + heading.y * travel};
			if (world.Body(static_cast<int>(std::round(origin.x)), static_cast<int>(std::round(origin.y))) != State::Open) return result;
			const auto passage = SweepBody(world, origin, axis, result.openDepth, settings.nearRange, Layer::Movement);
			if (passage.clearDepth < result.openDepth) return result;
		}
	}
	result.openingSafe = true;
	return result;
}
}
