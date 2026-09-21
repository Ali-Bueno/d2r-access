#pragma once

#include "native_ui.h"

#include <cstdint>

namespace D2RL { struct PluginContext; }

// What the focused inventory grid holds, cell by cell, logged whenever it
// changes -- the map the reader speaks from -- beside the loader's own list of
// where each of the player's inventory items sits. Diagnosis only, never
// spoken: it lets a cell the reader called empty be checked against the game.
// The question it exists for, 2026-09-21: arrows were refused ("I can't carry
// anymore") while cells in the inventory still read as empty, and the log had
// no way to say whether a free column of three was really there.
namespace D2RAccess::InventoryCensus {
void Log(const D2RL::PluginContext* context, NativeUi::Focus focus, uint64_t now);
}
