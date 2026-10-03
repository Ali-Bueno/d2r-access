#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <optional>

// The rows of a list that tells choosing from acting: the characters of
// character select. One click on a row chooses it, and the row chosen is the
// one Enter acts on, whatever the pointer is over. A second click on the same
// row within half a second acts on it, as Enter does. A list whose rows act
// on one click (a dropdown's, an NPC's menu) is not one of these.
// docs/keyboard-play.md
namespace D2RAccess::NativeListRows {
bool Initialize(const D2RL::PluginContext* context);

// True for a row that is chosen by one click and acted on by two.
bool TwoClicks(uintptr_t row);
// The row's place in its list, and the place of the row the list has chosen;
// -1 for the second when none is. Empty when either cannot be read.
struct Places { int row = 0, chosen = -1; };
std::optional<Places> Of(uintptr_t row);
}
