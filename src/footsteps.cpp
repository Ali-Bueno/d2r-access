#include "footsteps.h"

namespace D2RAccess::Footsteps {
bool Filter::Suppress(const Sample& sample, uint64_t now) {
	// NPC/other-player footsteps must neither be suppressed nor overwrite the
	// local player's baseline. Missing data restores normal sound.
	if (sample.valid && !sample.local) return false;
	if (!sample.valid || !sample.locomotion) { Reset(); return false; }
	// Design guards: allow a starting footfall and small animation scheduling
	// offsets; a one-second break starts a new sequence after pause/interaction.
	constexpr uint64_t StallMs = 150, SequenceGapMs = 1000;
	const bool rebase = !previous_.valid || sample.entity != previous_.entity
		|| now < lastSeen_ || now - lastSeen_ > SequenceGapMs;
	if (rebase || sample.position != previous_.position) lastMoved_ = now;
	previous_ = sample;
	lastSeen_ = now;
	return !rebase && now - lastMoved_ >= StallMs;
}
}
