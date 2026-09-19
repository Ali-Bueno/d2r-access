#include "keyboard_pad.h"
#include "test_support.h"

namespace {
using namespace D2RAccess::KeyboardPad;

bool Has(const Pad& pad, uint16_t bit) { return (pad.buttons & bit) != 0; }
}

int main() {
	// Nothing held is a centred, released pad, so a caller can tell an idle
	// keyboard from a deliberate push without inspecting every field.
	const Pad idle = Build({});
	Check(Idle(idle), "an untouched keyboard reports a centred, released pad");

	// The four face buttons, the two shoulders and the right stick click.
	Keys keys;
	keys.a = keys.b = keys.x = keys.y = true;
	keys.leftShoulder = keys.rightShoulder = keys.rightThumb = true;
	const Pad face = Build(keys);
	Check(Has(face, A) && Has(face, B) && Has(face, X) && Has(face, Y), "j n u i report the four face buttons");
	Check(Has(face, LeftShoulder) && Has(face, RightShoulder), "y and b report the shoulder buttons");
	Check(Has(face, RightThumb), "the apostrophe reports the right stick click");
	Check(!Idle(face), "a held button is not idle");

	// Triggers are analog on a pad but binary on a keyboard: a held key has to
	// clear whatever threshold the game uses to tell a pull from a rest.
	keys = {};
	keys.leftTrigger = true;
	const Pad trigger = Build(keys);
	Check(trigger.leftTrigger == TriggerPressed && trigger.rightTrigger == 0, "t pulls only the left trigger, fully");

	// XInput's Y axis grows upward, so W is positive Y, not negative.
	keys = {};
	keys.moveUp = true;
	const Pad forward = Build(keys);
	Check(forward.leftThumbY == ThumbMax && forward.leftThumbX == 0, "W pushes the left stick fully forward");
	keys = {};
	keys.moveLeft = true;
	Check(Build(keys).leftThumbX == -ThumbMax, "A pushes the left stick fully left");

	// A diagonal travels the stick's circle, not the corner of a square.
	keys = {};
	keys.moveUp = keys.moveRight = true;
	const Pad diagonal = Build(keys);
	Check(diagonal.leftThumbX == ThumbDiagonal && diagonal.leftThumbY == ThumbDiagonal, "W+D reads as one diagonal push, not an over-range one");
	Check(diagonal.leftThumbX < ThumbMax, "a diagonal does not exceed a real stick's deflection");

	// Opposite keys cancel, as they must on a stick that cannot be pushed both
	// ways at once; otherwise a stuck key would pin the character to one side.
	keys = {};
	keys.moveUp = keys.moveDown = true;
	Check(Build(keys).leftThumbY == 0, "W and S together centre the left stick");
	keys = {};
	keys.lookLeft = keys.lookRight = true;
	Check(Build(keys).rightThumbX == 0, "K and semicolon together centre the right stick");

	// The scan cluster is the right stick, and must not disturb movement.
	keys = {};
	keys.lookUp = true;
	const Pad look = Build(keys);
	Check(look.rightThumbY == ThumbMax, "O pushes the right stick forward");
	Check(look.leftThumbX == 0 && look.leftThumbY == 0, "the scan cluster leaves movement centred");

	// The arrow keys are the D-pad, so menu navigation is the game's own.
	keys = {};
	keys.dpadUp = keys.dpadLeft = true;
	const Pad dpad = Build(keys);
	Check(Has(dpad, DpadUp) && Has(dpad, DpadLeft), "the arrow keys report the D-pad");
	Check(dpad.leftThumbX == 0 && dpad.leftThumbY == 0, "the D-pad is separate from the left stick");

	Passed();
	return 0;
}
