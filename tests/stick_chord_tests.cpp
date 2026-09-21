#include "stick_chord.h"
#include "test_support.h"

int main() {
	using D2RAccess::StickChord::Fired;
	using D2RAccess::StickChord::Thumbs;
	const auto none = [](Fired f) { return !f.left && !f.right && !f.chord; };

	Thumbs thumbs;
	Check(none(thumbs.Update(true, false)), "pressing L3 fires nothing yet: it may still become a chord");
	const auto left = thumbs.Update(false, false);
	Check(left.left && !left.right && !left.chord, "releasing it alone is an L3 click");

	Check(none(thumbs.Update(false, true)), "R3 likewise waits for its release");
	const auto right = thumbs.Update(false, false);
	Check(right.right && !right.left && !right.chord, "and is an R3 click");

	Check(none(thumbs.Update(true, false)), "L3 down first");
	const auto chord = thumbs.Update(true, true);
	Check(chord.chord && !chord.left && !chord.right, "R3 joining it is the chord, at once");
	Check(none(thumbs.Update(false, true)), "releasing L3 after a chord is not an L3 click");
	Check(none(thumbs.Update(false, false)), "nor is releasing R3 an R3 click");

	const auto together = thumbs.Update(true, true);
	Check(together.chord, "both pressed on the same sample is a chord too");
	Check(none(thumbs.Update(true, true)), "held, it fires once");
	Check(none(thumbs.Update(false, false)), "and both released together fire nothing more");

	Check(none(thumbs.Update(false, true)), "a fresh R3 press after the chord");
	Check(thumbs.Update(false, false).right, "is an ordinary R3 click again");
	Passed();
}
