#pragma once

#include "native_rooms.h"
#include "radar.h"

#include <vector>

// Where the ground the game has never streamed begins, seen from the player:
// on open levels the radar's floor reading is "unexplored" in the screen
// quadrant that holds reachable frontier. The frontier is a walkable cell of
// the loaded collision whose neighbour lies in a logical room never seen; the
// seen memory only grows, so a quadrant can only go quiet, never flicker.
// Pure logic over the collision World and room rectangles. `docs/exploration.md`
namespace D2RAccess::Frontier {
// The screen quadrant of a world offset from the player: the direction whose
// screen axis dominates, ties going to the horizontal one.
Radar::Direction Quadrant(double dx, double dy);
// One flag per screen direction: reachable frontier lies that way. `unseen`
// holds the subtile rectangles of the level's rooms never streamed.
Radar::Marks Quadrants(const Radar::World& world, const std::vector<NativeRooms::Bounds>& unseen);
}
