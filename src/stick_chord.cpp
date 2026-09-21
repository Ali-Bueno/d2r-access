#include "stick_chord.h"

namespace D2RAccess::StickChord {

Fired Thumbs::Update(bool left, bool right, uint64_t nowMs) {
	Fired fired;
	if (right && !right_) rightSinceMs_ = nowMs;
	if (left && right && !chorded_) {
		chorded_ = true;
		fired.chord = true;
	}
	if (left_ && !left && !chorded_) fired.left = true;
	if (right_ && !right && !chorded_) {
		// The release decides which of the two R3 gestures this press was.
		if (nowMs - rightSinceMs_ >= RightHoldMs) fired.rightHold = true;
		else fired.right = true;
	}
	left_ = left;
	right_ = right;
	if (!left && !right) chorded_ = false;
	return fired;
}

}
