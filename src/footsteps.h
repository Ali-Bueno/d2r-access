#pragma once
#include "percent_steps.h"
#include <array>
#include <cstdint>

namespace D2RAccess::Footsteps {
struct Sample {
	uint32_t entity = 0;
	uint64_t position = 0; // packed native 16.16 X/Y, including every fractional bit
	bool valid = false, local = false, locomotion = false;
	uint32_t mode = ~0U; // native animation mode, diagnostic only
	bool pushing = false; // the game refused a stick push since the last local footfall
};
// Pushing: treading into a wall, which the stall rule alone missed when the
// character restarted its walk or crept along a ragged border (a tester).
enum class Reason { Unrelated, Unavailable, Animation, Baseline, Moved, Settling, Stalled, Pushing, Count };
// Less advance than this between footfalls while pushing is no step: a quarter subtile in 16.16.
inline constexpr int64_t PushStepMinimum = int64_t{1} << 14;
struct Decision {
	Reason reason = Reason::Unrelated;
	uint64_t stationaryMs = 0;
	int64_t dx = 0, dy = 0; // signed change in native 16.16 coordinates
};
const char* ReasonName(Reason reason);
// The player's own footsteps as a percent of the game's. A sound instance keeps
// its volume in 0-255, so a step grows only while it fits (docs/footsteps.md).
inline constexpr auto VolumePercents = PercentSteps::Quarters<25, 300>();
inline constexpr int NeutralVolumePercent = 100, MaxInstanceVolume = 255;
inline constexpr int NeutralVolumeLevel = PercentSteps::LevelOf(VolumePercents, NeutralVolumePercent);
static_assert(VolumePercents[NeutralVolumeLevel] == NeutralVolumePercent);
// A stock instance volume scaled by `percent`, kept within 1 and MaxInstanceVolume.
int ScaleVolume(int stock, int percent);
// The floor step is already at MaxInstanceVolume, so a louder one is made of extra
// footfalls: `whole` at the game's volume plus one at `partialPercent` of it.
struct Extras { int whole = 0, partialPercent = 0; };
Extras ExtraVoices(int percent);
class Filter {
public:
	bool Suppress(const Sample& sample, uint64_t now);
	const Decision& LastDecision() const { return decision_; }
	void Reset() { *this = {}; }
private:
	Decision decision_{};
	Sample previous_{};
	Sample last_{}; // the last local footfall of any animation, for the push check
	uint64_t lastSeen_ = 0, lastMoved_ = 0;
};
}
