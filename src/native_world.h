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
// Takes `mask` off the one cell x,y of the client's map with the game's own
// single-cell reset 0x366130(room, x, y, mask): the routine its stamp remover
// 0x3636D0 calls for each cell of a footprint and for the marker at its centre.
// It finds the cell's room among the player's room and its neighbours
// (0x38DFB0) and does nothing when there is none. For a marker a unit left
// behind (radar.h LeftMarkers). False when the routine is not the one this was
// read from, or the player cannot be read. UI thread only.
bool ClearCell(int x, int y, uint16_t mask) noexcept;
// Factored for fault/structure tests. Performs only guarded reads.
bool ReadPlayer(uintptr_t player, uint32_t playerId, uint64_t session, int level, Radar::World& output, Diagnostics* diagnostics = nullptr);
}
