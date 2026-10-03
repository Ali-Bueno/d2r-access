#pragma once

#include "native_ui.h"

#include <cstdint>
#include <string>

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

// In keyboard mode a row's skill icons and the two gold lines are places of
// the cursor as well (the user, 2026-10-01: "the player should be able to
// highlight the individual skill icons and read them like tooltips"). An icon
// is called by its skill, read from the icon as its tooltip is written, and
// Shift or Ctrl with an arrow walks that tooltip's lines as it walks an item's.
//
// UI thread, every poll while a menu owns focus. True while the hire list has
// focus: capture is kept on, and the left stick is this reader's while Y is held.
bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus);
// What the cursor says as it comes to a skill icon or a gold line of the hire
// list: the skill's name, with the rest of its tooltip when details are read
// unasked; the game's own words for the gold line, and the amount. Empty for
// anything else, a row included: the menu reader names a row.
std::string Said(uintptr_t widget, uintptr_t panel);
void Reset();

}
