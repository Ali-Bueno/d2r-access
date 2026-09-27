#pragma once

#include "native_ui.h"

#include <D2RLPlugin/api.h>
#include <optional>

// The rows of the belt above the one that is always shown. A belt holds 4 to
// 16 potions, by the belt worn, four to a row; slot = column + 4 * row, and
// row 0 is the bottom row, the one the keys 1 to 4 drink from. The game draws
// the rows above it, and takes a click on them, only while the belt is shown
// in full: while the pointer is over it, or while the game's own "show belt"
// (the grave accent key) is on. The keyboard's cursor turns that on for as
// long as the belt is in its menu. docs/keyboard-play.md
namespace D2RAccess::NativeBeltRows {
inline constexpr int Columns = 4, MostSlots = 16;

bool Initialize(const D2RL::PluginContext* context);
bool Available();
// How many slots the belt worn has: 4, 8, 12 or 16. 0 when unreadable.
int Slots(uintptr_t belt);
// Whether the belt is drawn in full now.
bool Expanded(uintptr_t belt);
// Whether the game's "show belt" is on; none when unreadable.
std::optional<bool> Shown();
// UI thread. Runs what the game's "show belt" key runs, which turns it on or
// off, and does nothing with no belt worn. False when it could not be run.
bool ToggleShown(const D2RL::PluginContext* context);
// The slot the belt itself takes a click on that point for, -1 for none.
int SlotAt(uintptr_t belt, NativeUi::Cell point);
}
