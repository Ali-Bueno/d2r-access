#include "stick_chord.h"
#include "test_support.h"

int main() {
	using D2RAccess::StickChord::Fired;
	using D2RAccess::StickChord::RightHoldMs;
	using D2RAccess::StickChord::Thumbs;
	const auto none = [](Fired f) { return !f.left && !f.right && !f.rightHold && !f.chord; };

	uint64_t now = 1000; // only the differences matter
	Thumbs thumbs;
	Check(none(thumbs.Update(true, false, now)), "pressing L3 fires nothing yet: it may still become a chord");
	const auto left = thumbs.Update(false, false, now);
	Check(left.left && !left.right && !left.chord, "releasing it alone is an L3 click");

	Check(none(thumbs.Update(false, true, now)), "R3 likewise waits for its release");
	const auto right = thumbs.Update(false, false, now + RightHoldMs - 1);
	Check(right.right && !right.rightHold && !right.chord, "released short of the threshold it is an R3 click");

	now += RightHoldMs;
	Check(none(thumbs.Update(false, true, now)), "a second R3 press");
	Check(none(thumbs.Update(false, true, now + RightHoldMs)), "fires nothing while it is still down");
	const auto hold = thumbs.Update(false, false, now + RightHoldMs);
	Check(hold.rightHold && !hold.right && !hold.chord, "and is a hold once released on the threshold");

	now += 2 * RightHoldMs;
	Check(none(thumbs.Update(true, false, now)), "L3 down first");
	const auto chord = thumbs.Update(true, true, now);
	Check(chord.chord && !chord.left && !chord.right, "R3 joining it is the chord, at once");
	Check(none(thumbs.Update(false, true, now + RightHoldMs)), "releasing L3 after a chord is not an L3 click");
	Check(none(thumbs.Update(false, false, now + RightHoldMs)), "nor is its long-held R3 a hold or a click");

	const auto together = thumbs.Update(true, true, now);
	Check(together.chord, "both pressed on the same sample is a chord too");
	Check(none(thumbs.Update(true, true, now)), "held, it fires once");
	Check(none(thumbs.Update(false, false, now)), "and both released together fire nothing more");

	Check(none(thumbs.Update(false, true, now)), "a fresh R3 press after the chord");
	Check(thumbs.Update(false, false, now).right, "is an ordinary R3 click again");
	Passed();
}
