#pragma once

#include "attack_reach.h"

#include <D2RLPlugin/api.h>
#include <optional>

// The player's attack loadout read from the game: bound skills and the weapons in hand.
namespace D2RAccess::NativeAttack {
// Nothing when neither the skills nor the equipment can be read. UI thread only.
std::optional<AttackReach::Loadout> ReadLoadout(const D2RL::PluginContext* context);
}
