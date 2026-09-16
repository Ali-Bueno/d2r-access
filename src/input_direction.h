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

enum class Vertical {
	None,
	Up,
	Down,
};

// Samples all input sources once; call from the UI poll (see
// ui_thread_pump.h). Records a press EDGE per source (not-held -> held), so
// holding a direction down does not keep re-triggering it. Cheap, does not
// allocate, never throws.
void Poll();

// Cardinal D-pad input only; independent of sticks/keyboard and sampled once
// per UI tick. Diagonals remain held but do not choose an arbitrary direction.
struct DpadInput { int x = 0, y = 0; bool pressed = false, held = false; };
DpadInput Dpad();

// The most recent left/right press edge, if it happened within the last
// `maxAgeMs` milliseconds (measured via GetTickCount64); Horizontal::None
// otherwise, or if no edge has been recorded yet.
Horizontal LastHorizontal(unsigned maxAgeMs);

// Same for up/down: a list that scrolls under a still focus indicator only
// reveals which way it moved through the press that caused it.
Vertical LastVertical(unsigned maxAgeMs);

// Gamepad face buttons. Some screens open a modal from a face button without
// any UI message saying which action it was (character select: X deletes);
// the press that preceded the modal tells.
enum class FaceButton {
	None,
	A,
	B,
	X,
	Y,
};
FaceButton LastFaceButton(unsigned maxAgeMs);

// Releases the dynamically loaded XInput library. Safe to call even if Poll
// was never called.
void Shutdown();

}
