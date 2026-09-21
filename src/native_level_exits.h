#pragma once
#include "radar.h"
#include <cstdint>
#include <vector>

namespace D2RAccess::NativeLevelExits {
struct Exit { uint32_t warp = 0, destination = 0; Radar::Position position; };
// Walk existing logical rooms in the current level, including inactive rooms.
// No native generator, room loader, map reveal or memory write is involved.
// Empty/unavailable presets are skipped. A malformed traversal returns no exits.
bool Read(uintptr_t activeRoom, std::vector<Exit>& output);
// Coordinate identity survives native warp-unit unloading/recreation. Zero is
// reserved for entities that are not fixed level exits.
uint64_t Identity(Radar::Position position);

// A logical room's tile rectangle (RoomEx +0x60..+0x6C).
struct TileRect { int x = 0, y = 0, width = 0, height = 0; };
// A room of this level touching a room of another level.
struct BorderPair { TileRect room, neighbour; uint32_t destination = 0; };
// A walk-through border: the neighbouring level and the subtile midpoint of the
// edge the two rooms share, which is where the automap draws the way across.
struct BorderExit { uint32_t destination = 0; Radar::Position position; };
// One exit per neighbouring level, at the shared edge nearest the player.
std::vector<BorderExit> Borders(const std::vector<BorderPair>& pairs, Radar::Position player);
// Border exits of the current level. Maze levels are excluded: their rooms are
// LvlPrest cells of one level and every way out is a real warp.
bool ReadBorders(uintptr_t activeRoom, Radar::Position player, std::vector<BorderExit>& output);
}
