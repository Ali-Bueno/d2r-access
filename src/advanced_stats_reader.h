#pragma once

#include "input_direction.h"
#include "native_ui.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <string>
#include <vector>

// The Advanced Stats panel (AdvancedStatsPanel), on the right stick, the same
// way the character panel's statistics are:
//
//   up    previous line     left   first line
//   down  next line         right  last line
//
// The panel is a scrolling list that nothing can focus -- MEASURED
// 2026-09-22, the only focus it ever reports is its ScrollBar -- so until now
// only its title was spoken. The View button opens it from both the
// character panel and the mercenary panel, and the game copies the header
// (name, class or type and level, experience) from whichever it came from.
//
// Opened from the character panel: the advanced list only. The character
// panel's own statistics stay in its own reader, which is not used while this
// panel is open.
//
// Opened from the mercenary panel it is the only place that mercenary's
// statistics and skills can be read, since its own panel shares the screen
// with the inventory (MercenaryStats). The lines are then everything:
//
//   header, the mercenary panel's statistics, each skill's description,
//   then the advanced list.
namespace D2RAccess::AdvancedStatsReader {

// The mercenary panel the open Advanced Stats panel belongs to, or 0 when it
// belongs to the character.
uintptr_t Mercenary();

// Every line described above. Empty when the panel is not visible.
std::vector<std::string> Lines(uintptr_t panel, uintptr_t mercenary);

// True when a stick push was claimed, so the readers further down the tick
// leave the stick alone. Runs ahead of the character panel's reader, whose
// panel may still be open behind this one.
bool Tick(const D2RL::PluginContext* context, NativeUi::Focus focus);

void Reset();

}
