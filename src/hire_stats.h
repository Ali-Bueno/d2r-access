#pragma once

#include "native_ui.h"

#include <cstdint>

namespace D2RL { struct PluginContext; }

// A mercenary's stats in the hire list, on the right stick the way the
// character panel's are (the user, 2026-09-22: "the right stick should act as a
// buffer as on the character panel so that mercenary stats can be read"). The
// list row gives the name, level and cost, which the menu reader already says;
// the rest is only in the game's tooltip for the highlighted row (ui.json has
// its "Hold Shift to Compare" and the controller's "Show Mercenary Skills"),
// so the tooltip's lines are what the stick walks:
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
