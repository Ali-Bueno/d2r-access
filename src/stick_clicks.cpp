#include "stick_clicks.h"

namespace D2RAccess::StickClicks {

Fired Gesture::Update(bool down, uint64_t nowMs) {
	Fired fired;
	if (down && !down_) {
		since_ = nowMs;
		consumed_ = false;
		if (pending_ && nowMs - released_ <= DoubleClickMs) {
			pending_ = false;
			consumed_ = true; // the release of the second press is part of this gesture
			fired.doubleClick = true;
		}
	} else if (down && !consumed_ && nowMs - since_ >= HoldMs) {
		consumed_ = true;
		fired.hold = true;
	} else if (!down && down_ && !consumed_) {
		pending_ = true;
		released_ = nowMs;
	}
	down_ = down;
	if (pending_ && nowMs - released_ > DoubleClickMs) {
		pending_ = false;
		fired.click = true;
	}
	return fired;
}

void Gesture::Reset() { *this = Gesture(); }

}
