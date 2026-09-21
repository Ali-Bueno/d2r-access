#pragma once

#include "native_rooms.h"
#include "navigation_grid.h"
#include "radar.h"

#include <cstddef>
#include <vector>

// A level-wide coarse map, for the half of the level the collision window
// cannot reach.
//
// native_world.cpp keeps a 152-subtile snapshot around the player, because the
// game itself only allocates collision (Room1) near the player and frees it
// again. So Navigation::Build can never see further, and every catalogued
// object beyond it has steps == -1. Beacon::FindRoute then falls back to the
// reachable cell nearest the target, which is a straight line with local
// avoidance -- in a winding dungeon that heading can point confidently into a
// dead end.
//
// The level's LOGICAL rooms, however, all exist from the moment the level
// does, with their tile rectangles, whether or not collision has been built
// for them. That is enough for a room-level route, which turns "walk toward a
// point four hundred subtiles away" into "walk into the next room", and the
// fine grid still does all the real obstacle avoidance inside the window.
//
// Pure logic over rooms and a Grid: no native calls, no SDK, unit-tested
// without the game.
namespace D2RAccess::RoomGraph {

// The game's own definition of "near", reimplemented rather than invented.
//
// D2R 3.3.93787 RVA 0x3614C0 (653 bytes) rebuilds a room's near list by
// walking every room in the level and keeping those whose gap, measured
// between facing edges and negative where the rectangles overlap, is under six
// TILES on both axes:
//
//     gapX = (self.x < other.x) ? other.x - (self.x + self.w)
//                               : self.x - (other.x + other.w);
//     gapY = ... same on Y ...
//     if (gapX < 6 && gapY < 6) near.push_back(other);
//
// That function is already fingerprinted in entity_layout.h's Checks table
// (hash 0xA85E778CC89FCD70), so LayoutCheck::Verify refuses an image whose
// bytes differ -- the rule below cannot silently drift from the game's.
//
// IT IS A PROXIMITY TEST, NOT A PASSABILITY TEST. The game uses the list to
// decide which rooms to consider; two rooms can satisfy it across a solid
// wall. Nothing here may treat a room route as a proved path -- that is what
// ChooseSubGoal and the collision grid are for.
inline constexpr int NearGapTiles = 6;
bool Near(const NativeRooms::Room& a, const NativeRooms::Room& b);

// Adjacency over every room in the level. Self-edges are dropped: the native
// list includes the room itself (its own gap is negative), which is useful to
// the game and only noise to a route.
struct Graph {
	std::vector<NativeRooms::Room>    rooms;
	std::vector<std::vector<size_t>>  near; // indices into rooms
	bool Empty() const { return rooms.empty(); }
};
Graph Build(const std::vector<NativeRooms::Room>& rooms);

// Breadth-first room route, inclusive of both ends, nearest in room-hops.
// Empty when an index is out of range or no chain of near rooms links them;
// a single element when from == to.
std::vector<size_t> Route(const Graph& graph, size_t from, size_t to);

// Where to actually walk next.
//
// Never a point chosen by geometry -- always a cell the navigation grid has
// already proved reachable, so a sub-goal can no more point through a wall
// than the grid can. Preference order:
//
//   1. the reachable cell inside the NEXT room on the route with the fewest
//      steps: by construction the nearest way into that room, i.e. its
//      doorway, not its middle or its far side;
//   2. failing that -- the next room has no collision yet -- the reachable
//      cell in the CURRENT room closest to the next room's rectangle, which
//      walks the player at the boundary they have to cross;
//   3. failing that, nothing, and the caller keeps whatever it was doing.
struct SubGoal {
	bool             known = false;
	size_t           room = NativeRooms::None; // the room the sub-goal lies in
	Radar::Position  position;                 // subtile cell, always reachable
	int              steps = -1;
	bool             inNextRoom = false;       // false when case 2 applied
};
SubGoal ChooseSubGoal(const Graph& graph, const Navigation::Grid& grid, const std::vector<size_t>& route);

}
