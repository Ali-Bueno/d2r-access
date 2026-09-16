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
Guidance EvaluateGuidance(const World&, Position, const Probe& terrain, const Probe& movement,
	Position, const Settings& settings, uint64_t playbackMs) {
	Guidance result;
	result.playbackMs = playbackMs;
	result.warningDepth = settings.blockedDepth;
	// Physical clearance is for readout and validating openings. Only the
	// connected passage tracker generates sounds. Speed and queue
	// length must never turn the same wall into a newly open passage.
	result.openDepth = settings.openDepth;
	result.openingSafe = movement.clearDepth >= result.openDepth && movement.medianDepth >= result.openDepth;
	if (!terrain.available) return result;
	if (terrain.end == State::Blocked && terrain.clearDepth <= result.warningDepth) {
		result.observed = State::Blocked;
		return result;
	}
	if (terrain.clearDepth < result.openDepth || terrain.medianDepth < result.openDepth) return result;
	result.observed = State::Open;
	return result;
}
}
