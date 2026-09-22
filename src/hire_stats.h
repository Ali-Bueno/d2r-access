#pragma once

#include "native_ui.h"

#include <cstdint>

namespace D2RL { struct PluginContext; }

// What the hire list shows of a mercenary beyond its row, on the right stick
// the way the character panel's stats are (the user, 2026-09-22). The row gives
// the name, level and cost, which the menu reader already says, and three skill
// icons; the layout has no life, damage or defense. The skills' tooltips are
// drawn only while Y is held (MEASURED 2026-09-22), and their lines are what
// the stick walks -- the right stick, or while Y is held the left one:
//
//   up    previous line     left   first line
//   down  next line         right  last line
//
// A menu suspends the interaction reader, and with it tooltip capture, so this
// keeps capture on while the hire list has focus.
namespace D2RAccess::HireStats {

// UI thread, every poll while a menu owns focus. True while the hire list has
// focus: capture is kept on and the right stick is this reader's.
bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus);
void Reset();

}
