#include "stick_chord.h"

namespace D2RAccess::StickChord {

Fired Thumbs::Update(bool left, bool right) {
	Fired fired;
	if (left && right && !chorded_) {
		chorded_ = true;
		fired.chord = true;
	}
	if (left_ && !left && !chorded_) fired.left = true;
	if (right_ && !right && !chorded_) fired.right = true;
	left_ = left;
	right_ = right;
	if (!left && !right) chorded_ = false;
	return fired;
}

}
