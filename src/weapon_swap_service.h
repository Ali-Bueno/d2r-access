#pragma once
#include <D2RLPlugin/api.h>

// Wiring around WeaponSwap: a weapon swap (R3, W, the inventory's I/II tabs)
// names the set and the weapons now in hand. `access-weapon-swap on|off`.
namespace D2RAccess::WeaponSwapService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled();
void SetEnabled(bool enabled);
// Whether the swap leads with "Weapon set 1/2"; on by default (the user, 2026-10-12).
bool SetNumber();
void SetSetNumber(bool enabled);
}
