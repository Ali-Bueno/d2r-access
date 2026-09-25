#pragma once

#include <cstdint>

// The game says nothing when it refuses to equip an item: the item simply stays
// in hand. So a press of A on an equipment slot that leaves the same item in
// hand once the game has had time to answer was refused. Pure state.
namespace D2RAccess::EquipAttempt {

// How recent an A press still counts as the one on the slot under focus: a few
// UI polls (33 ms each), well short of AnswerMs so one press arms once.
inline constexpr unsigned PressWindowMs = 150;
// Time the game gets to take the item. It answers within frames offline; the
// rest is room for a server round trip.
inline constexpr uint64_t AnswerMs = 500;

class Tracker {
public:
	// One UI tick. `pressed`: A was just pressed on an equipment slot. `held`:
	// the runtime id of the item in hand, 0 when the hand is empty or unknown.
	// Returns true once, when a pending attempt was refused.
	bool Update(bool pressed, uint32_t held, uint64_t nowMs);
	void Reset();

private:
	uint32_t item_ = 0;
	uint64_t deadlineMs_ = 0;
};

}
