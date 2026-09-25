#pragma once

#include "native_rooms.h"
#include "navigation_grid.h"
#include "radar.h"

#include <cstddef>
#include <vector>

// A level-wide coarse map for what the collision window cannot reach: the
// level's logical rooms all exist from the start, collision only near the
// player. Pure logic, unit-tested without the game. docs/room-routing.md.
namespace D2RAccess::RoomGraph {

// The game's own nearness rule (RVA 0x3614C0, fingerprinted in entity_layout.h):
// facing-edge gap under six TILES on both axes. Proximity, not passability.
inline constexpr int NearGapTiles = 6;
bool Near(const NativeRooms::Room& a, const NativeRooms::Room& b);

// Rooms that share part of an edge, or overlap: the only way to walk from one into the other.
bool Touching(const NativeRooms::Room& a, const NativeRooms::Room& b);

// What the grid's collision proves about the shared edge of two touching rooms.
enum class Passage { Unknown, Open, Closed };
Passage Border(const Navigation::Grid& grid, const NativeRooms::Room& a, const NativeRooms::Room& b);

struct Graph {
	std::vector<NativeRooms::Room>   rooms;
	std::vector<std::vector<size_t>> links; // touching rooms, as indices into rooms
	bool Empty() const { return rooms.empty(); }
};
Graph Build(const std::vector<NativeRooms::Room>& rooms);

// Room hops from each room to `to`, skipping links `grid` proves closed; NoRoute where none.
inline constexpr int NoRoute = -1;
std::vector<int> Distances(const Graph& graph, size_t to, const Navigation::Grid* grid = nullptr);

// Where to walk next from room `from` toward room `to`: always a cell the grid
// proved reachable, never a point chosen by geometry.
struct SubGoal {
	bool             known = false;
	size_t           room = NativeRooms::None; // the room the sub-goal lies in
	Radar::Position  position;                 // subtile cell, always reachable
	int              steps = -1;
	bool             inNextRoom = false;       // inside a nearer room; false at the edge of unloaded ground
};
SubGoal ChooseSubGoal(const Graph& graph, const Navigation::Grid& grid, size_t from, size_t to);

}
