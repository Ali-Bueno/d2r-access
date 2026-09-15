#include "radar.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
Guidance EvaluateGuidance(const World& world, Position axis, const Probe& terrain, const Probe& movement,
	Position velocity, const Settings& settings) {
	Guidance result;
	const double speed = std::hypot(velocity.x, velocity.y);
	// A new event can arrive just after playback started, with three other
	// directions pending. Include all four cues and their gaps, then reaction.
	constexpr double QueueSeconds = DirectionCount * (CueDurationMs + CueGapMs) / 1000.0;
	const double reaction = settings.reactionMs / 1000.0;
	const double horizon = reaction + QueueSeconds;
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
	if (movement.clearDepth < result.openDepth || movement.medianDepth < result.openDepth) return result;
	const double advance = speed * horizon;
	// Forecast only within our acquired local range. Missing coverage rejects a
	// promise of an opening; it never invents an obstacle or an exit.
	if (advance > settings.farRange || result.openDepth > settings.farRange) return result;
	if (speed > 0) {
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
