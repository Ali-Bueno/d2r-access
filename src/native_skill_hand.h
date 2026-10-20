#pragma once

#include <D2RLPlugin/api.h>

// The hand the game's list of skills is open for. The list is one panel,
// SkillSelect, for both hands: the game's key S opens it for the right hand
// and a click on the HUD's left skill button for the left, and a click on a
// skill of the list puts it on that hand. The panel does not hold the hand;
// the game keeps it in its UI registry and reads it with a function of its
// own (0x238240), which is what is called here. docs/keyboard-play.md
namespace D2RAccess::NativeSkillHand {
enum class Hand { None, Left, Right };

bool Initialize(const D2RL::PluginContext* context);
bool Available();
// UI thread. None when the game has not chosen one, or the read failed.
Hand Chosen();
// UI thread, fresh visible/on-screen left HUD button only. Invoke its verified
// native click callback synchronously, without pointer movement or queued mouse
// input. Reject other button types/sides; retain the game's toggle semantics.
bool ActivateLeft(uintptr_t button) noexcept;
}
