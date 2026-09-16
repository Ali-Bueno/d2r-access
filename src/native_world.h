#pragma once

#include "radar.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::NativeWorld {
struct Diagnostics {
	const char* stage = "not-read";
	uint32_t playerId = 0;
	int x = 0, y = 0, pattern = -1, level = 0;
	uint32_t footprintMask = 0;
	uint32_t movementMask = 0;
	uint16_t rawPlayerCell = 0;
	unsigned removedPresenceCells = 0;
	int presenceOffsetX = 0, presenceOffsetY = 0;
};
bool Initialize(const D2RL::PluginContext* context);
// Compare against the same verified native local-player lookup used by Read.
bool IsLocalPlayer(uintptr_t player) noexcept;
// UI thread only, client rooms only. No retained game pointers across ticks.
// level == 0 permits bootstrapping from the verified player's native room.
bool Read(uint32_t playerId, uint64_t session, int level, Radar::World& output, Diagnostics* diagnostics = nullptr);
// Factored for fault/structure tests. Performs only guarded reads.
bool ReadPlayer(uintptr_t player, uint32_t playerId, uint64_t session, int level, Radar::World& output, Diagnostics* diagnostics = nullptr);
}
