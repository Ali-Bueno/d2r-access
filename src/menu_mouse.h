#pragma once

#include "native_ui.h"

#include <cstdint>

// The mouse as the keyboard's hand in a menu. In keyboard mode every act in a
// menu is a mouse act to the game: a click on a button or a tab, a click on an
// item to pick it up, a click with Shift or Ctrl held to equip or sell it. The
// mod puts the pointer on what its own cursor is on and clicks there, as the
// user's Diablo mod does (the user, 2026-09-27). docs/keyboard-play.md
//
// A click is a press and a release some frames apart, and a pointer that was
// just moved is clicked only after the game has seen the move: the steps are
// kept in a short queue that Tick works through. UI thread only.
namespace D2RAccess::MenuMouse {
enum class Button { Left, Right };

// True when a point of the game's client pixels lies in the game's window and the window is in
// front: the pointer goes nowhere else, so a click never lands on another window.
bool Within(NativeUi::Cell point);
// A small rectangle in the client's bottom-left corner; no size while the game is not in front.
NativeUi::Rect BottomCorner();
// True when the pointer stands on a point of the game's client pixels.
bool At(NativeUi::Cell point);
// The pointer on the middle of the rectangle, now, or on the part of it nearest the middle that a
// screen shows: a window can hang off the screen (2026-11-03, a game at 307,133 and 2560 wide).
// False when none of it can be pointed at.
bool Place(const NativeUi::Rect& rect);
// A move of two pixels and back, as a hand on the mouse makes: Place moves the
// pointer without an input event, and the game kept its hover where it was.
// MEASURED 2026-09-28: skills showed no tooltip and took no F-key until this
// (the user: "key binding is working now"); a waypoint was read with the Close
// button's tooltip. A relative move of one count changed nothing (03:25).
// Given after every landing of the cursor.
void Nudge();
// A click where the pointer is. With a key, the button stays down until that key is let go:
// the game's press-and-hold buttons see the hold (deleting a character).
void Click(Button button, int heldBy = 0);
// The pointer on the rectangle, a click there, and the pointer back on
// `after` when that has any size: a key that presses a button for the player.
void ClickAt(const NativeUi::Rect& rect, Button button, const NativeUi::Rect& after = {}, int heldBy = 0);
// The pointer on the rectangle and the wheel turned there, up for a positive number of notches.
void WheelAt(const NativeUi::Rect& rect, int notches);
// The pointer on the rectangle once every step that waits is done.
void PlaceAfter(const NativeUi::Rect& rect);
// True while steps wait: what the pointer is on is about to change.
bool Busy();
void Tick(uint64_t now);
void Reset();
}
