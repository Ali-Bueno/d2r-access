#pragma once

#include <cstdint>

// The two stick buttons, L3 and R3, read as four gestures: an L3 click, an R3
// click, an R3 hold, and both pressed together -- the chord that opens the
// terrain overview (the user, 2026-09-21: "put it on l3/r3 chord"). One press
// never fires two of them: a single click or hold is reported on RELEASE, and
// only when the other stick was not pressed at any point during it; the chord
// fires the moment both are down, and every release that follows it is
// swallowed until both are up again. Reporting on release costs a click's own
// length, about 100 ms, and is the only way to know a press did not become a
// chord. Pure state, unit-tested without a pad.
namespace D2RAccess::StickChord {

// R3 held at least this long is a hold, not a slow click: a deliberate click
// lasts roughly 100-150 ms. Main's original exit review used the same value
// (commit f17a6e6, `kRightStickHoldMs`).
inline constexpr uint64_t RightHoldMs = 350;

struct Fired {
	bool left = false;      // L3 clicked on its own
	bool right = false;     // R3 clicked on its own
	bool rightHold = false; // R3 held past RightHoldMs, then released
	bool chord = false;     // L3 and R3 pressed together
};

class Thumbs {
public:
	// One sample of both buttons, from the input poll. Returns what fired now.
	Fired Update(bool left, bool right, uint64_t nowMs);

private:
	bool left_ = false, right_ = false;
	bool chorded_ = false;      // the presses under way have already made a chord
	uint64_t rightSinceMs_ = 0; // when the R3 press under way went down
};

}
