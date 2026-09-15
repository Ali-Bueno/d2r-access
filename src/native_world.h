#pragma once

#include "radar.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::NativeWorld {
bool Initialize(const D2RL::PluginContext* context);
// UI thread only, client rooms only. No retained game pointers across ticks.
bool Read(uint32_t playerId, uint64_t session, int level, Radar::World& output);
// Factored for fault/structure tests. Performs only guarded reads.
bool ReadPlayer(uintptr_t player, uint32_t playerId, uint64_t session, int level, Radar::World& output);
}
