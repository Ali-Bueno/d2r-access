#pragma once

#include <cstdint>

namespace D2RL {
struct PluginContext;
}

// R3 is the guidance toggle during play, and it is also D2R's own weapon swap.
// MEASURED 2026-09-21: locking guidance on Kashya swapped the weapons 440 ms
// later ([equip] worn 4:1 5:2 -> 11:1 12:2). The SDK's input actions bind
// keyboard keys only, so the one way to stop the game acting on the same click
// is to take the click out of what the game reads.
//
// This replaces pointers, not code: the game image's own slot for
// XInputGetState, which the game's pad poll jumps through. The mod reads its
// pad through a pointer of its own (input_direction.cpp), so it still sees
// every click. The replacement calls the original and hands the
// game exactly what came back, with one exception -- while the mod claims R3,
// that button's bit is cleared. The pad is never reported absent, idle or on
// another slot, and no slot is ever occupied: every other button, stick and
// trigger passes through byte for byte (CRITICAL RULE 3). Clearing R3 during
// play is the user's call, 2026-09-21: "make sure right stick is consumed
// properly by the mod so it doesn't trigger weapon swap".
namespace D2RAccess::PadFilter {

// Swaps the slot. When the game has not filled it yet, ClaimRightThumb looks
// again for a while; either way the log says what happened. Safe to call
// twice.
bool Install(const D2RL::PluginContext* context);
void Remove();

// Whether the mod owns R3 right now: during play, and not in a menu, where the
// game uses the click itself (the inventory's "Hold to Sort"). Set from the UI
// tick. Also logs, once per click, that one was kept from the game.
void ClaimRightThumb(const D2RL::PluginContext* context, bool claimed);

// One pad's R3 press as the filter has seen it.
struct Press {
	bool down = false;
	bool kept = false; // this press is being kept from the game
};

// Pure: the buttons the game is handed. A press is decided on its first frame
// and kept to its release, so a click that began in a menu reaches the game
// whole and one that began in play never reaches it at all, whatever the claim
// does while it is held.
uint16_t Filter(uint16_t buttons, bool claimed, Press& press);

}
