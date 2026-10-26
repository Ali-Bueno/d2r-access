#pragma once

#include <D2RLPlugin/api.h>

#include <string>

// The hit chances the character sheet shows over Attack Rating and Defense, from the game's own
// tooltip builders: against the monster it last measured them by, Fallen at the start of a game.
// docs/character-panel.md#hit-chance-tooltips-2026-10-25
namespace D2RAccess::NativeHitChance {
bool Initialize(const D2RL::PluginContext* context);
// The tooltip over a skill block's Attack Rating, the left block's or the right's ("Fallen: 95%
// Average chance to hit"), cleaned for speech; empty when the game shows none. UI thread.
std::string Attack(bool left);
// The tooltip over Defense ("Chance to Block: 20% Average chance Fallen will hit you: 5%"). UI thread.
std::string Defense();
}
