#include "keyboard_pad.h"

namespace D2RAccess::KeyboardPad {

namespace {

// One stick, from four keys. The pair on each axis cancels, so holding both
// halves of an axis reads as centred rather than as whichever key was polled
// last -- the same thing a physical stick does when it is let go.
void Stick(bool up, bool down, bool left, bool right, int16_t& outX, int16_t& outY) noexcept {
	const int x = (right ? 1 : 0) - (left ? 1 : 0);
	const int y = (up ? 1 : 0) - (down ? 1 : 0);
	const int16_t magnitude = (x != 0 && y != 0) ? ThumbDiagonal : ThumbMax;
	outX = static_cast<int16_t>(x * magnitude);
	outY = static_cast<int16_t>(y * magnitude);
}

void Set(uint16_t& buttons, bool held, uint16_t bit) noexcept {
	if (held) buttons |= bit;
}

}

Pad Build(const Keys& keys) noexcept {
	Pad pad;
	Set(pad.buttons, keys.a, A);
	Set(pad.buttons, keys.b, B);
	Set(pad.buttons, keys.x, X);
	Set(pad.buttons, keys.y, Y);
	Set(pad.buttons, keys.leftShoulder, LeftShoulder);
	Set(pad.buttons, keys.rightShoulder, RightShoulder);
	Set(pad.buttons, keys.rightThumb, RightThumb);
	Set(pad.buttons, keys.dpadUp, DpadUp);
	Set(pad.buttons, keys.dpadDown, DpadDown);
	Set(pad.buttons, keys.dpadLeft, DpadLeft);
	Set(pad.buttons, keys.dpadRight, DpadRight);

	// The triggers are analog on a real pad, but a key is either down or up,
	// so a held key reports a fully pulled trigger -- past any threshold the
	// game applies to tell a pull from a rest position.
	pad.leftTrigger  = keys.leftTrigger ? TriggerPressed : uint8_t {0};
	pad.rightTrigger = keys.rightTrigger ? TriggerPressed : uint8_t {0};

	Stick(keys.moveUp, keys.moveDown, keys.moveLeft, keys.moveRight, pad.leftThumbX, pad.leftThumbY);
	Stick(keys.lookUp, keys.lookDown, keys.lookLeft, keys.lookRight, pad.rightThumbX, pad.rightThumbY);
	return pad;
}

bool Idle(const Pad& pad) noexcept {
	return pad.buttons == 0 && pad.leftTrigger == 0 && pad.rightTrigger == 0
		&& pad.leftThumbX == 0 && pad.leftThumbY == 0
		&& pad.rightThumbX == 0 && pad.rightThumbY == 0;
}

}
