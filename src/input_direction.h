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

// Right thumbstick push edges for the entity scan: a push is recorded once
// when the stick leaves its dead zone (same threshold as the left stick), in
// the dominant axis at that moment. Releasing below the threshold re-arms it,
// so cycling means push, release, push. Take() returns the latest push since
// the previous Take() and clears it.
enum class StickPush {
	None,
	Up,
	Left,
	Right,
	Down,
};
StickPush TakeRightStickPush();

// Right thumbstick CLICK (R3), reported on RELEASE when the press stayed
// shorter than the hold threshold below. The SDK's action service binds
// keyboard keys only (D2RLPlugin/input.h Key/Modifier), so the pad button is
// read here with the rest of the XInput state. Take() returns whether a click
// happened since the previous Take() and clears it.
bool TakeRightStickClick();

// Right thumbstick HOLD (R3 kept down past the click threshold). Fires once
// per press, on reaching the threshold, and the release that follows is not
// reported as a click, so the two gestures never both fire.
bool TakeRightStickHold();

// Left thumbstick of the first connected pad as sampled by the last Poll(),
// in screen convention (x right, y down), each axis in [-1, 1]. Diagnostics
// only: the radar logs it next to each cue so a log can tell where the player
// was steering when the cue fired. Zero and not connected without a pad.
struct Stick { float x = 0, y = 0; bool connected = false; };
Stick LeftStick();

// Releases the dynamically loaded XInput library. Safe to call even if Poll
// was never called.
void Shutdown();

}
