#include "radar.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
uint64_t PlaybackBudget(const AudioLoad& load, const Events& candidates, size_t direction) {
	const auto scheduled = load.pending[direction] != State::Unknown ? load.pending[direction] : load.playing[direction];
	if (load.remainingMs[direction] && (candidates[direction] == State::Unknown || candidates[direction] == scheduled)) {
		// Existing cues keep their place and elapsed playback time. Starting a
		// fresh full-batch forecast each poll could invalidate the second side.
		uint64_t remaining = load.remainingMs[direction];
		for (size_t i = 0; i < DirectionCount; ++i)
			if (i != direction && candidates[i] != State::Unknown && load.pending[i] == State::Unknown) remaining += CueSlotMs;
		return remaining;
	}
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
	const double reaction = settings.reactionMs / 1000.0;
	const double horizon = reaction + playbackMs / 1000.0;
	const double approach = std::max(0.0, velocity.x * axis.x + velocity.y * axis.y);
	result.warningDepth = std::max(settings.blockedDepth, approach * (horizon + settings.stableMs / 1000.0));
	// Describe the passage available now. Running changes warning timing, not
	// whether an existing side passage counts as an opening.
	result.openDepth = settings.openDepth;
	if (!terrain.available) return result;
	if (terrain.end == State::Blocked && terrain.clearDepth <= result.warningDepth) {
		result.observed = State::Blocked;
		return result;
	}
	if (terrain.clearDepth < result.openDepth || terrain.medianDepth < result.openDepth) return result;
	result.observed = State::Open;
	return result;
}
Guidance EvaluateGuidance(const World&, Position axis, const Probe& terrain, const Probe& movement,
	Position velocity, const Settings& settings, uint64_t playbackMs) {
	auto result = ClassifyGuidance(axis, terrain, velocity, settings, playbackMs);
	// Kirandur reports current geometry. The old future-straight-line veto
	// hid usable turns merely because continuing forward would miss them or
	// hit a wall. Keep the complete native body and current occupancy guard.
	result.openingSafe = movement.clearDepth >= result.openDepth && movement.medianDepth >= result.openDepth;
	return result;
}
}
