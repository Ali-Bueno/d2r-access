#pragma once
#include "ground_items.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::NativeGroundItems {
bool Initialize(const D2RL::PluginContext* context);
// Verified local player unit and its table bank; shared with NativeEntities.
bool RuntimeRoots(uintptr_t& player, uintptr_t& tables) noexcept;

// Gold the local player is carrying, or -1 when it cannot be read. The
// inventory's gold button carries no text of its own -- MEASURED 2026-09-20,
// [field] focus name="gold_button" ... text="" -- so the amount has to come
// from the player's stats, the same UNIT_GetStat call a gold pile on the
// ground already uses.
int PlayerGold() noexcept;
bool Read(uint32_t playerId, uint64_t session, GroundItems::Snapshot& output);
// Guarded snapshot adapter, also exercised using constructed native memory.
bool ReadPlayer(uintptr_t player, uintptr_t tables, uint32_t playerId, uint64_t session, GroundItems::Snapshot& output);
}
