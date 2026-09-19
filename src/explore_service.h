#pragma once

#include <D2RLPlugin/api.h>

// Nearest unexplored space, on demand. The service remembers where the player
// has walked on this level (Explored::Map, painted from its own poll), and on a
// key press routes to the nearest place that memory does not cover, speaking
// the way there as pushes of the left stick (UnexploredRoute). Borrowed from
// the same feature in the author's DevilutionX fork, where it is the key blind
// players use to sweep a dungeon.
namespace D2RAccess::ExploreService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context);
void Unregister();
}
