#pragma once
#include "ground_items.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::NativeGroundItems {
bool Initialize(const D2RL::PluginContext* context);
bool Read(uint32_t playerId, uint64_t session, GroundItems::Snapshot& output);
// Guarded snapshot adapter, also exercised using constructed native memory.
bool ReadPlayer(uintptr_t player, uintptr_t tables, uint32_t playerId, uint64_t session, GroundItems::Snapshot& output);
}
