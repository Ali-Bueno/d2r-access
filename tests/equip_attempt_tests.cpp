#include "equip_attempt.h"
#include "test_support.h"

int main() {
	using D2RAccess::EquipAttempt::AnswerMs;
	using D2RAccess::EquipAttempt::Tracker;
	constexpr uint32_t Sword = 7, Helm = 9;
	uint64_t now = 1000; // only the differences matter
	Tracker tracker;
	Check(!tracker.Update(false, Sword, now), "holding an item is not an attempt");
	Check(!tracker.Update(true, 0, now), "a press with an empty hand is not an attempt");

	Check(!tracker.Update(true, Sword, now), "a press on a slot arms an attempt");
	Check(!tracker.Update(false, Sword, now + AnswerMs - 1), "the game gets its time to answer");
	Check(tracker.Update(false, Sword, now + AnswerMs), "the same item still in hand was refused");
	Check(!tracker.Update(false, Sword, now + 2 * AnswerMs), "and it is reported once");

	now += 10 * AnswerMs;
	Check(!tracker.Update(true, Sword, now), "another attempt");
	Check(!tracker.Update(false, 0, now + 10), "an emptied hand means the item was taken");
	Check(!tracker.Update(false, 0, now + AnswerMs), "so nothing is reported");

	now += 10 * AnswerMs;
	Check(!tracker.Update(true, Sword, now), "a third attempt");
	Check(!tracker.Update(false, Helm, now + 10), "a swap puts the old item in hand");
	Check(!tracker.Update(false, Helm, now + AnswerMs), "which is not a refusal");

	now += 10 * AnswerMs;
	Check(!tracker.Update(true, Sword, now), "an attempt");
	tracker.Reset();
	Check(!tracker.Update(false, Sword, now + AnswerMs), "leaving the inventory forgets it");
	Passed();
}
