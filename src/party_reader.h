#pragma once

#include "native_ui.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <string>
#include <vector>

// The multiplayer party panel (PartyPanel) on the right stick, as the character
// panel's statistics are (a tester, 2026-10-14: its buttons were read, its
// members not): up and down step, left and right go to the first and the last.
// The lines: the game's name and password, then each player's row, its name,
// its information line and whether it is in a party.
namespace D2RAccess::PartyReader {

// Every line in screen order; empty when the panel is not visible.
std::vector<std::string> Lines(uintptr_t panel);

// True when a right-stick push was the panel's.
bool Tick(const D2RL::PluginContext* context, NativeUi::Focus focus);
void Reset();

}
