#pragma once
#include "radar.h"
#include <string>

namespace D2RAccess::Navigation {
// The widest radius Build accepts. Past it the game keeps no collision to read
// -- the native snapshot is a 152-subtile window around the player
// (native_world.cpp) -- so a wider grid would be empty cells, not more level.
// Build returns an EMPTY grid for anything larger rather than clamping it, so
// a caller that asks for more gets nothing at all and every object in it reads
// as unreachable. Pin the request to this constant, never to a number of your
// own.
inline constexpr int MaxRadius = 150;
// World-grid axes. These bits are not the radar's fixed screen directions.
enum DirectionBits : uint8_t {
	MinusX = 1, PlusX = 2, MinusY = 4, PlusY = 8,
	MinusXMinusY = 16, PlusXMinusY = 32, MinusXPlusY = 64, PlusXPlusY = 128
};
struct Cell {
	Radar::State body = Radar::State::Unknown;
	uint16_t flags = 0; // original centre-cell flags, not a semantic object ID
	uint16_t blocker = 0; // flags of the cell that blocks the body here (the centre when clear)
	int room = -1; // streamed room index, see LabelRooms
	uint8_t exits = 0, firstSteps = 0;
	int steps = -1; // shortest count of native neighboring destination steps
	int clearance = 0; // Chebyshev distance to blocked/unknown body cells or window limit
};
struct Grid {
	int x = 0, y = 0, width = 0, height = 0;
	int playerX = 0, playerY = 0;
	uint64_t session = 0;
	int level = 0, bodyPattern = 0;
	uint16_t walkingMask = 0;
	bool originKnown = false, touchesUnknown = false, touchesWindow = false;
	size_t reachable = 0;
	std::vector<Cell> cells;
	const Cell* At(int worldX, int worldY) const;
	std::string Dump() const; // P player, . reachable, o disconnected, # blocked, ? unknown
};
// Reads the supplied world without modifying it; no native calls or persistence.
// Callers choose physical or structural snapshots. Radius is in collision subtiles.
// The terrain layer ignores the units standing on the floor.
Grid Build(const Radar::World& world, int radius, Radar::Layer layer = Radar::Layer::Movement);
// Stamps each cell with the index of the room covering it in `rooms`, which
// may be the physical world behind a structural (single-room) snapshot.
void LabelRooms(Grid& grid, const Radar::World& rooms);
}
