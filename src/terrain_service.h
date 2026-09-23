#pragma once

#include "radar.h"
#include <D2RLPlugin/api.h>

// Speaks the terrain overview on demand and keeps the exploration memory: the
// rooms the game has streamed, the rooms the player stood in, on disk across
// sessions. Lifecycle and speech plumbing around TerrainOverview::Describe.
// `docs/terrain-overview.md`, `docs/exploration.md`
namespace D2RAccess::TerrainService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
// Stamps the radar's world with what the memory knows: the rooms stood in,
// the exits into levels never seen, the frontier quadrants of open ground.
void Qualify(Radar::World& world);
// True when the player has stood in the room whose rect origin is given, in
// tiles, on this level. The room announcer tags its ways out with it.
bool Visited(int level, int tileX, int tileY);
bool Known(int level); // at least one room of that level seen
}
