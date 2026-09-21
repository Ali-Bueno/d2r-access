#pragma once

#include <cstdint>

// The three gestures the mod reads from the right stick button (R3): a click,
// a double click and a hold. A single click is only reported once the
// double-click window has passed without a second press, so one press can
// never fire two of them. Pure timing, unit-tested without a pad.
// `docs/terrain-overview.md`
namespace D2RAccess::StickClicks {
// A deliberate click lasts roughly 100-150 ms, so holding past this is
// unambiguously a hold and not a slow click, while still firing soon enough
// that waiting for it does not feel slow.
inline constexpr uint64_t HoldMs = 350;
// Both presses of a double click land inside this window; a single click is
// held back for it, which is the price of telling the two apart.
inline constexpr uint64_t DoubleClickMs = 300;

struct Fired { bool click = false, doubleClick = false, hold = false; };

class Gesture {
public:
	// One sample of the button, from the input poll. Returns what fired now.
	Fired Update(bool down, uint64_t nowMs);
	void Reset();
private:
	bool down_ = false;
	bool consumed_ = false; // this press already fired, so its release is not a click
	bool pending_ = false;  // a click waiting out the double-click window
	uint64_t since_ = 0, released_ = 0;
};
}
