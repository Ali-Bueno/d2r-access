#pragma once

#include <cstdint>

// When Act II's darkness falls, told from the client's own flag. docs/quests.md
namespace D2RAccess::EclipseWatch {
// The server repeats the flag as an act loads; the quest's own fall comes about 12 s after the
// player reaches its levels. A rise sooner than this after an act loads is not the sun going out.
inline constexpr uint64_t ActSettleMs = 10'000;

class Watch {
public:
	// True when the land has just gone dark. `act` is the act the client holds.
	bool Update(uint64_t now, uintptr_t act, bool dark) {
		bool fell = !dark_ && dark && act == act_ && now - actSince_ >= ActSettleMs;
		if (act != act_) {
			act_ = act;
			actSince_ = now;
		}
		dark_ = dark;
		return fell;
	}

private:
	uintptr_t act_ = 0;
	uint64_t actSince_ = 0;
	bool dark_ = false;
};
}
