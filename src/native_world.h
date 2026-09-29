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
// The player's native collision room -- the entry point the level's own
// logical room list is reached through (native_rooms.h). Zero when the player
// cannot be resolved. Guarded reads only; nothing is retained.
uintptr_t PlayerRoom() noexcept;
// Takes the player's stamp off the client map centred at x,y with the game's
// own remover 0x3636D0(room, x, y, pattern, footprint mask), the call it
// makes itself before moving a unit; pattern and mask are the player's own.
// For a stamp the game left where the player no longer stands (radar.h
// LeftPlayerStamps). UI thread only.
bool RemovePlayerStamp(int x, int y) noexcept;
// Factored for fault/structure tests. Performs only guarded reads.
bool ReadPlayer(uintptr_t player, uint32_t playerId, uint64_t session, int level, Radar::World& output, Diagnostics* diagnostics = nullptr);
}
