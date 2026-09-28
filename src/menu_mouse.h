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

// The pointer on the middle of the rectangle, now. False when it could not be put there.
bool Place(const NativeUi::Rect& rect);
// A move of two pixels and back, as a hand on the mouse makes: Place moves the
// pointer without an input event. EXPERIMENT 2026-09-28 for the skills of the
// skill tree and of the list of skills, whose tooltip, and F1 to F8, came only
// when something made the game look at what is under the pointer again: a
// panel opening, or a click, which is input too (MEASURED 03:25:44). The first
// try, a relative move of one count, changed nothing (MEASURED 03:25).
void Nudge();
// A click where the pointer is.
void Click(Button button);
// The pointer on the rectangle, a click there, and the pointer back on
// `after` when that has any size: a key that presses a button for the player.
void ClickAt(const NativeUi::Rect& rect, Button button, const NativeUi::Rect& after = {});
// The pointer on the rectangle once every step that waits is done.
void PlaceAfter(const NativeUi::Rect& rect);
// True while steps wait: what the pointer is on is about to change.
bool Busy();
void Tick(uint64_t now);
void Reset();
}
