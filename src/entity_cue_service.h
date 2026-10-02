#pragma once

#include <D2RLPlugin/api.h>
#include <optional>

namespace D2RAccess::EntityCueService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
// How far the player's attacks reach, in subtiles; nothing outside a game or before the first read.
std::optional<double> CurrentReach();
// How far a monster the HUD plate names may stand and still be named: the melee
// reach. Nothing for a ranged attack, whose shot reaches what the auto-target picks.
std::optional<double> NamingReach();
}
