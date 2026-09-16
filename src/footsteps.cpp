#include "footsteps.h"

namespace D2RAccess::Footsteps {
const char* ReasonName(Reason reason) {
	switch (reason) {
	case Reason::Unavailable: return "unavailable";
	case Reason::Animation: return "animation";
	case Reason::Baseline: return "baseline";
	case Reason::Moved: return "moved";
	case Reason::Settling: return "settling";
	case Reason::Stalled: return "stalled";
	default: return "unrelated";
	}
}
bool Filter::Suppress(const Sample& sample, uint64_t now) {
	decision_ = {};
	// An unresolved foreign entity must not reset the local player's baseline.
	// A failed read of that same full ECS ID still restores normal local audio.
	if (!sample.local && (sample.valid || !previous_.valid || sample.entity != previous_.entity)) return false;
	if (!sample.valid || !sample.locomotion) {
		Reset();
		decision_.reason = sample.valid ? Reason::Animation : Reason::Unavailable;
		return false;
	}
	// Design guards: allow a starting footfall and small animation scheduling
	// offsets; a one-second break starts a new sequence after pause/interaction.
	constexpr uint64_t StallMs = 150, SequenceGapMs = 1000;
	const bool rebase = !previous_.valid || sample.entity != previous_.entity
		|| now < lastSeen_ || now - lastSeen_ > SequenceGapMs;
	if (!rebase) {
		decision_.dx = static_cast<int64_t>(static_cast<uint32_t>(sample.position)) - static_cast<uint32_t>(previous_.position);
		decision_.dy = static_cast<int64_t>(sample.position >> 32) - static_cast<int64_t>(previous_.position >> 32);
	}
	if (rebase || sample.position != previous_.position) lastMoved_ = now;
	decision_.stationaryMs = now - lastMoved_;
	decision_.reason = rebase ? Reason::Baseline : sample.position != previous_.position ? Reason::Moved
		: decision_.stationaryMs >= StallMs ? Reason::Stalled : Reason::Settling;
	previous_ = sample;
	lastSeen_ = now;
	return decision_.reason == Reason::Stalled;
}
}
