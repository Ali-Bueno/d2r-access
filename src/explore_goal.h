#pragma once

#include "navigation_grid.h"
#include "radar.h"
#include "room_graph.h"

#include <cstdint>
#include <vector>

// Where to walk to find new ground: the nearest reachable room not yet
// entered, by actual walking steps when loaded floor provides a route,
// otherwise by room hops through ground still unseen. Pure logic over the level's room graph and the exploration
// memory, unit-tested without the game.
namespace D2RAccess::ExploreGoal {
using Radar::Position;

struct Choice {
	bool found = false;
	size_t room = NativeRooms::None; // index into the graph's rooms
	Position position; // nearest reachable entry cell, otherwise the room centre, in subtiles
	int hops = 0;                    // rooms between the player and it, itself included
};

// `explored` holds actual visits/current room (TerrainService::ExplorationRooms);
// `refused` holds the ids of rooms a walk has already given up on, so the next
// ask moves on instead of trying the same wall again. A border the grid proves
// closed is not walked through. Nothing is found when every room that can be
// reached is explored, or when the player stands in no room of the graph.
Choice Nearest(const RoomGraph::Graph& graph, const Navigation::Grid& grid,
	const std::vector<NativeRooms::Room>& explored, Position player,
	const std::vector<uint64_t>& refused = {});
}
