#pragma once

#include <D2RLPlugin/api.h>

// The game's own weapon swap. Its key is W, which is the keyboard's right hand
// in play, so the mod gives the swap a key of its own and runs what the game's
// key runs: the handler of key action 44 (0x11BF00). That handler asks the
// game's own two questions (is input taken by something else, is the character
// free to act), reads the set in use and hands it to the swap (0x1E33A0), which
// tells the server. The game decides; a swap it refuses changes nothing.
// docs/weapon-swap.md, docs/keyboard-play.md
namespace D2RAccess::NativeSwap {
enum class Result { Asked, NoPlayer, Unavailable };

bool Initialize(const D2RL::PluginContext* context);
bool Available();
// UI thread. Asked means the game's handler ran, not that the weapons changed:
// the set in use (NativeWeaponSet::SecondSet) changes when they have.
Result Swap(const D2RL::PluginContext* context);
}
