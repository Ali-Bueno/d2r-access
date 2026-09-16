#pragma once
#include <cstdint>

namespace D2RAccess::Footsteps {
struct Sample {
	uint32_t entity = 0;
	uint64_t position = 0; // packed native 16.16 X/Y, including every fractional bit
	bool valid = false, local = false, locomotion = false;
	uint32_t mode = ~0U; // native animation mode, diagnostic only
};
enum class Reason { Unrelated, Unavailable, Animation, Baseline, Moved, Settling, Stalled, Count };
struct Decision {
	Reason reason = Reason::Unrelated;
	uint64_t stationaryMs = 0;
	int64_t dx = 0, dy = 0; // signed change in native 16.16 coordinates
};
const char* ReasonName(Reason reason);
class Filter {
public:
	bool Suppress(const Sample& sample, uint64_t now);
	const Decision& LastDecision() const { return decision_; }
	void Reset() { *this = {}; }
private:
	Decision decision_{};
	Sample previous_{};
	uint64_t lastSeen_ = 0, lastMoved_ = 0;
};
}
