#pragma once
#include <cstdint>

namespace D2RAccess::Footsteps {
struct Sample {
	uint32_t entity = 0;
	uint64_t position = 0; // packed native 16.16 X/Y, including every fractional bit
	bool valid = false, local = false, locomotion = false;
};
class Filter {
public:
	bool Suppress(const Sample& sample, uint64_t now);
	void Reset() { *this = {}; }
private:
	Sample previous_{};
	uint64_t lastSeen_ = 0, lastMoved_ = 0;
};
}
