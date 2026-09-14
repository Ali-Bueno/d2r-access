#pragma once

// Value controls (sliders, dropdowns) fire a UI update event when the player
// changes them, but the event carries no value, only that the control
// changed. To speak "increased"/"decreased" the plugin infers the direction
// from whichever input source must have just driven the change: the most
// recent left/right press across the gamepad D-pad, the gamepad left
// thumbstick, and the keyboard arrow keys.
namespace D2RAccess::InputDirection {

enum class Horizontal {
	None,
	Left,
	Right,
};

// Samples all input sources once; call from the UI poll (see
// ui_thread_pump.h). Records a press EDGE per source (not-held -> held), so
// holding a direction down does not keep re-triggering it. Cheap, does not
// allocate, never throws.
void Poll();

// The most recent left/right press edge, if it happened within the last
// `maxAgeMs` milliseconds (measured via GetTickCount64); Horizontal::None
// otherwise, or if no edge has been recorded yet.
Horizontal LastHorizontal(unsigned maxAgeMs);

// Releases the dynamically loaded XInput library. Safe to call even if Poll
// was never called.
void Shutdown();

}
