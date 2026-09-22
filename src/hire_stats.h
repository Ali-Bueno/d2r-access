#pragma once

#include "native_ui.h"

#include <cstdint>

namespace D2RL { struct PluginContext; }

// A mercenary's skills in the hire list, walked on the left stick while Y is
// held, the way the character panel's stats are on the right stick (the user,
// 2026-09-22). The row gives the name, level and cost, which the menu reader
// already says, and three skill icons; the row code references nothing else
// (READ: HireName, strchrlvl, HireLevel, HireCost, Skill%d) and no tooltip is
// drawn for a highlighted row. Holding Y draws one tooltip per skill (MEASURED
// 2026-09-22), and their lines are what the left stick walks:
//
//   up    previous line     left   first line
//   down  next line         right  last line
//
// A menu suspends the interaction reader, and with it tooltip capture, so this
// keeps capture on while the hire list has focus.
namespace D2RAccess::HireStats {

// UI thread, every poll while a menu owns focus. True while the hire list has
// focus: capture is kept on, and the left stick is this reader's while Y is held.
bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus);
void Reset();

}
