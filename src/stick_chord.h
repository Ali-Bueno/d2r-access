#pragma once

// The two stick buttons, L3 and R3, read as three gestures: an L3 click, an R3
// click, and both pressed together -- the chord that opens the terrain
// overview (the user, 2026-09-21: "put it on l3/r3 chord"). One press never
// fires two of them: a single click is reported on RELEASE, and only when the
// other stick was not pressed at any point during it; the chord fires the
// moment both are down, and every release that follows it is swallowed until
// both are up again. Reporting on release costs a click's own length, about
// 100 ms, and is the only way to know a press did not become a chord.
// Pure state, unit-tested without a pad.
namespace D2RAccess::StickChord {

struct Fired {
	bool left = false;  // L3 clicked on its own
	bool right = false; // R3 clicked on its own
	bool chord = false; // L3 and R3 pressed together
};

class Thumbs {
public:
	// One sample of both buttons, from the input poll. Returns what fired now.
	Fired Update(bool left, bool right);

private:
	bool left_ = false, right_ = false;
	bool chorded_ = false; // the presses under way have already made a chord
};

}
