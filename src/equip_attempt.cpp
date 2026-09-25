#include "equip_attempt.h"

namespace D2RAccess::EquipAttempt {

bool Tracker::Update(bool pressed, uint32_t held, uint64_t nowMs) {
	if (item_) {
		// Placed, swapped or dropped: the hand changed, so the game answered.
		if (held != item_) { item_ = 0; return false; }
		if (nowMs < deadlineMs_) return false;
		item_ = 0;
		return true;
	}
	if (pressed && held) {
		item_ = held;
		deadlineMs_ = nowMs + AnswerMs;
	}
	return false;
}

void Tracker::Reset() { item_ = 0; }

}
