#pragma once

#include "native_level_areas.h"
#include "radar.h"

#include <cstdint>
#include <vector>

// Which other areas of the act touch the one the player is standing in, and
// where the crossing into each of them is.
//
// This is the answer to "how do I get out of here" on a map whose way out is
// not a warp -- a town, or one wilderness running into the next. See
// native_level_areas.h for why those have no warp to read.
//
// The crossing point is a property of the two rectangles alone, not of where
// the player happens to be, so it does not move underfoot: it is the middle of
// the span the two areas share, on the edge of the neighbour nearest this one.
//
// IT IS A BEARING, NOT A DOORWAY. The shared edge of two areas is mostly solid
// -- a town's palisade, a cliff -- and the gap through it is somewhere along
// that edge, not necessarily at its middle. What this gives is the direction
// the neighbour lies in and roughly how far, which is what a player sweeping
// the level on foot is missing. The collision grid still does the real routing
// once the way through is inside its window.
//
// Pure logic over rectangles: no native calls, no SDK, unit-tested without the
// game.
namespace D2RAccess::LevelAreas {

struct Crossing {
	uint32_t        level = 0;
	Radar::Position position;   // subtiles, on the neighbour's near edge
	int             gapTiles = 0; // 0 when the areas touch or overlap
};

// The game's own "near" rule for rooms, applied to whole areas: a gap under
// six tiles on both axes. Reimplemented in RoomGraph::Near from the native
// function at RVA 0x3614C0, which entity_layout.h fingerprints. An area
// further off than that -- a cave the act placed at its own offset, thousands
// of tiles away -- is not somewhere the player can walk to from here.
std::vector<Crossing> Neighbours(const std::vector<NativeLevelAreas::Area>& areas, uint32_t currentLevel);

}
