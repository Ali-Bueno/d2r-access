#pragma once

#include <D2RLPlugin/api.h>
#include <cstdint>

// The game's own sorting of a grid of items. The controller has it on a held
// button ("Hold to Sort", ControllerPromptHoldToSortInventory); no layout has
// a button for it, so with the mouse there is nothing to click. The keyboard
// makes the three calls the grid's own handler makes when that hold completes
// (0x2C6FE4), with the same values read from the same grid: the game decides
// whether the page may be sorted, and sorts it. docs/keyboard-play.md
namespace D2RAccess::NativeSort {
enum class Result { Sorted, Refused, Unavailable };

bool Initialize(const D2RL::PluginContext* context);
bool Available();
// UI thread. `grid` is an InventoryGridWidget that is shown. Refused is the
// game's own answer: an item in hand, a merchant's cursor, a page that is not
// sorted. Nothing is said or changed by a refusal.
Result Sort(const D2RL::PluginContext* context, uintptr_t grid);
}
