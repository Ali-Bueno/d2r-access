#pragma once

#include "rooms.h"

// The map's sides a dungeon room's ways out are told by: the room's own walls, which D2
// draws at the screen diagonals (a tester, 2026-10-14: "passages at 2 o'clock"). World -Y is
// the northeast wall, +X the southeast, +Y the southwest, -X the northwest. Engine-free.
// `docs/rooms.md`
namespace D2RAccess::RoomSides {
using Radar::Position;

// A world offset as the map's side it points to most: one of the four world axes.
Position MapSide(Position offset);
// The wall a passage is in, outward: a world axis.
Position Wall(const Rooms::Opening& opening);
}
