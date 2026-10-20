#pragma once
#include "radar.h"
#include <cstddef>
#include <cstdint>
#include <vector>

// Every logical room of the current level, including the ones the game has not
// made active. The traversal is the same one native_level_exits.cpp already
// walks -- Level -> first RoomEx -> RoomNext, reading each tile rectangle --
// but it keeps the rectangles instead of discarding everything that is not a
// warp preset. No native generator, room loader, map reveal or memory write is
// involved, and a malformed traversal returns no rooms rather than a partial
// list.
namespace D2RAccess::NativeRooms {

inline constexpr size_t None = static_cast<size_t>(-1);

// Rectangles are in TILES, the unit the native fields use. Subtiles() converts
// to the unit the collision world and the navigation grid speak.
struct Room {
	int      tileX = 0, tileY = 0, tileWidth = 0, tileHeight = 0;
	uint64_t id = 0; // stable across visits; see Identity
};

// Subtile rectangle, max exclusive.
struct Bounds { int minX = 0, minY = 0, maxX = 0, maxY = 0; };
Bounds Subtiles(const Room& room);
bool   Contains(const Bounds& bounds, int x, int y);

// A room is identified by where it is, not by its address or its position in
// the native list: the pointer changes when the game frees and rebuilds a
// room, and the list order is a generation detail. Zero is reserved for "no
// room", so a room at the origin still gets a usable identity.
uint64_t Identity(int tileX, int tileY);

// Sorted by (tileY, tileX) so a room's number is the same on every visit,
// independent of the order the game happens to link them in. A nonzero `level`
// fails the read when the room is another level's: the player between levels.
bool Read(uintptr_t activeRoom, std::vector<Room>& output, int level = 0);

// Index of the room holding this subtile position, or None. Rooms do not
// overlap, so the first match is the only one.
size_t Containing(const std::vector<Room>& rooms, Radar::Position subtile);

}
