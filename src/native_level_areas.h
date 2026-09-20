#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Every level the act has already built, with the tile rectangle its rooms
// occupy.
//
// MEASURED, from the game's own Levels.txt (D2R 3.3, extracted from CASC):
// adjacent OUTDOOR areas are not joined by a warp. The Rogue Encampment lists
// no warp at all -- every Vis is 0 and every Warp is -1 -- and the Blood
// Moor's four warps all lead to the Den of Evil, never back to town. A warp in
// D2 is the way into or out of an ENCLOSED level: a cave, a crypt, a tower, a
// sewer. Walking from the encampment to the Blood Moor crosses a boundary
// between two areas that simply sit next to each other on the act's map.
//
// So the warp reader (native_level_exits.cpp) cannot answer "which way is out
// of town" -- there is nothing there for it to find, and that is the data
// being right rather than the reader being wrong. What does answer it is where
// the neighbouring area actually IS, and the act keeps that: every level it
// has instantiated is on one list, and each one's rooms carry their tile
// rectangles in the same world grid as the player's own.
//
// The offsets come from DrlgGetOrAddLevel, whose lookup walks exactly this
// list; that function is already fingerprinted in DrlgLayout::Checks, so the
// same verification that guards the map seed guards this too.
//
// No native generator, room loader, map reveal or memory write is involved. A
// malformed traversal returns no areas rather than a partial list.
namespace D2RAccess::NativeLevelAreas {

// The union of a level's room rectangles, in TILES.
struct Area {
	uint32_t level = 0;
	int      tileX = 0, tileY = 0, tileWidth = 0, tileHeight = 0;
	size_t   rooms = 0;
};

// Every instantiated level of the act, the player's own included. Levels that
// have no rooms yet are left out: an area with no rooms has no place on the
// map to point at.
bool Read(uintptr_t activeRoom, std::vector<Area>& output);

}
