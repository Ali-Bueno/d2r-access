#pragma once

#include <D2RLPlugin/api.h>

// The pad walks to what it aims at, as a click does: attacks skip the game's docking test,
// and an NPC is a target as far as the screen while nothing else is at hand.
// docs/input-commands.md
namespace D2RAccess::NativePadApproach {
bool Initialize(const D2RL::PluginContext* context);
bool Enabled();
void SetEnabled(bool on);
// UI thread.
void Tick(const D2RL::PluginContext* context);
}
