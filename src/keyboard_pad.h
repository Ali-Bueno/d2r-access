#pragma once

#include <cstdint>

// Keyboard play drives the game through the controller path D2R already has,
// instead of through a mod-side cursor. The game's own controller code owns
// menu focus, movement pathing and firing a skill at the target it picks, so
// the mod's job is to report the pad state those systems expect and then read
// what the game does with it -- never to move the game's focus itself.
//
// This header deliberately restates XInput's constants rather than including
// <windows.h>, so the mapping is a pure function that can be tested without a
// game, a pad, or a Windows message loop.
namespace D2RAccess::KeyboardPad {

// XINPUT_GAMEPAD_* button bits, value-for-value.
inline constexpr uint16_t DpadUp        = 0x0001, DpadDown      = 0x0002;
inline constexpr uint16_t DpadLeft      = 0x0004, DpadRight     = 0x0008;
inline constexpr uint16_t Start         = 0x0010, Back          = 0x0020;
inline constexpr uint16_t LeftThumb     = 0x0040, RightThumb    = 0x0080;
inline constexpr uint16_t LeftShoulder  = 0x0100, RightShoulder = 0x0200;
inline constexpr uint16_t A             = 0x1000, B             = 0x2000;
inline constexpr uint16_t X             = 0x4000, Y             = 0x8000;

// Full deflection of one stick axis (SHORT_MAX), and a fully pulled trigger.
inline constexpr int16_t ThumbMax       = 32767;
inline constexpr uint8_t TriggerPressed = 255;

// A stick held corner-to-corner never reaches full deflection on both axes at
// once -- it travels a circle, not a square. Reporting ThumbMax on both would
// claim a magnitude of 46341, past anything a real pad can produce, which a
// game is free to clamp in whatever direction it likes. ThumbMax / sqrt(2)
// keeps a diagonal the same distance from centre as a cardinal push, so eight
// directions all read as one deliberate, equal push.
inline constexpr int16_t ThumbDiagonal = 23170;

// One frame of the keyboard, already resolved to what each key stands for.
// The key each field is named after is in the comment; the layout itself is
// applied where the physical keyboard is sampled, not here.
struct Keys {
	bool a = false, b = false, x = false, y = false;                            // j n u i
	bool leftShoulder = false, rightShoulder = false;                           // y b
	bool leftTrigger = false, rightTrigger = false;                             // t v
	bool leftThumb = false, rightThumb = false;                                 // ' and /
	bool start = false, back = false;                                           // = and -
	bool moveUp = false, moveLeft = false, moveDown = false, moveRight = false; // w a s d
	bool lookUp = false, lookLeft = false, lookDown = false, lookRight = false; // o k l ;
	bool dpadUp = false, dpadDown = false, dpadLeft = false, dpadRight = false; // arrow keys
};

// The subset of XINPUT_GAMEPAD this mapping can produce.
struct Pad {
	uint16_t buttons      = 0;
	uint8_t  leftTrigger  = 0, rightTrigger = 0;
	int16_t  leftThumbX   = 0, leftThumbY   = 0;
	int16_t  rightThumbX  = 0, rightThumbY  = 0;
};

// The pad state a physical controller would report for these keys. Opposite
// keys on one axis cancel, as they do on a stick that cannot be pushed both
// ways at once. XInput's Y axis grows upward, so "up" is positive.
Pad Build(const Keys& keys) noexcept;

// True when nothing is held, so a caller can leave a real pad's state alone
// rather than overwriting it with an empty frame.
bool Idle(const Pad& pad) noexcept;

}
