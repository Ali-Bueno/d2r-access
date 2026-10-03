#pragma once

#include <D2RLPlugin/api.h>
#include <cstdint>

// The game's own filling of the belt from the inventory. The controller has
// it on a held button ("Hold to Populate Belt"); with the mouse there is
// nothing to click for it. The keyboard makes the calls the belt's own handler
// makes when that hold completes (0x2285FD): the game says whether there is
// anything to fill, makes the plan of which potion goes to which slot, and
// sends it to the server, which makes the same plan itself and refuses one
// that differs before it moves anything. docs/keyboard-play.md
namespace D2RAccess::NativeBeltFill {
enum class Result { Sent, Nothing, Refused, Unavailable };

bool Initialize(const D2RL::PluginContext* context);
bool Available();
// UI thread. `belt` is the HUD's BeltWidget, shown. Nothing is the game's own
// answer that no potion of the inventory has a slot to go to; Refused, that an
// item is in hand or the belt has no owner.
Result Fill(const D2RL::PluginContext* context, uintptr_t belt);
}
