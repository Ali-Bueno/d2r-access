#include "footsteps.h"
#include <algorithm>

namespace D2RAccess::Footsteps {
const char* ReasonName(Reason reason) {
	switch (reason) {
	case Reason::Unavailable: return "unavailable";
	case Reason::Animation: return "animation";
	case Reason::Baseline: return "baseline";
	case Reason::Moved: return "moved";
	case Reason::Settling: return "settling";
	case Reason::Stalled: return "stalled";
	case Reason::Pushing: return "pushing";
	default: return "unrelated";
	}
}
bool Filter::Suppress(const Sample& sample, uint64_t now) {
	decision_ = {};
	// An unresolved foreign entity must not reset the local player's baseline.
	// A failed read of that same full ECS ID still restores normal local audio.
	if (!sample.local && (sample.valid || !previous_.valid || sample.entity != previous_.entity)) return false;
	const Sample last = last_;
	if (sample.valid) last_ = sample;
	if (sample.valid && sample.locomotion && sample.pushing && last.valid && last.entity == sample.entity) {
		const int64_t dx = static_cast<int64_t>(static_cast<uint32_t>(sample.position)) - static_cast<uint32_t>(last.position);
		const int64_t dy = static_cast<int64_t>(sample.position >> 32) - static_cast<int64_t>(last.position >> 32);
		if (dx * dx + dy * dy < PushStepMinimum * PushStepMinimum) {
			decision_ = {Reason::Pushing, 0, dx, dy};
			previous_ = sample;
			lastSeen_ = now;
			return true;
		}
	}
	if (!sample.valid || !sample.locomotion) {
		const Sample kept = last_;
		Reset();
		last_ = kept;
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
int ScaleVolume(int stock, int percent) {
	const long long scaled = (static_cast<long long>(stock) * percent + NeutralVolumePercent / 2) / NeutralVolumePercent;
	return static_cast<int>(std::clamp<long long>(scaled, 1, MaxInstanceVolume));
}
Extras ExtraVoices(int percent) {
	if (percent <= NeutralVolumePercent) return {};
	const int extra = percent - NeutralVolumePercent;
	return {extra / NeutralVolumePercent, extra % NeutralVolumePercent};
}
}
