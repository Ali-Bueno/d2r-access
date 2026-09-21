#pragma once

#include <D2RLPlugin/api.h>

// Speaks the terrain overview on demand and keeps track of which rooms of the
// level the game has streamed. Lifecycle and speech plumbing around
// TerrainOverview::Describe. `docs/terrain-overview.md`
namespace D2RAccess::TerrainService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
}
