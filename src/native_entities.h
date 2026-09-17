#pragma once
#include "entity_radar.h"
#include <D2RLPlugin/api.h>
#include <cstdint>
#include <vector>

// Live monsters / NPCs / world objects in the local player's room and its
// neighbours, with the game's own localized names. Guarded reads only; the
// native name resolver is never called because it allocates.
namespace D2RAccess::NativeEntities {
bool Initialize(const D2RL::PluginContext* context);
// Distant presets are opt-in for the directional scan/beacon. Proximity speech
// and spatial loops keep the nearby, live-unit-only snapshot.
bool Read(uint32_t playerId, uint64_t session, std::vector<EntityRadar::Entity>& output, bool distantExits = false);
// Factored for tests over constructed native memory.
bool ReadPlayer(uintptr_t player, uintptr_t tables, uint32_t playerId, uint64_t session, std::vector<EntityRadar::Entity>& output, bool distantExits = false);
}
