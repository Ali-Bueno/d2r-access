#pragma once

#include "navigation_grid.h"
#include "level_route.h"
#include "radar.h"
#include "room_graph.h"

#include <cstdint>
#include <vector>

// Where to walk to find new ground: uncovered walking floor first, then a
// streaming frontier or an openable door. The same floor search serves every
// act. The older room selector remains for its independent graph tests.
namespace D2RAccess::ExploreGoal {
using Radar::Position;

struct Choice {
	bool found = false;
	size_t room = NativeRooms::None; // index into the graph's rooms
	Position position; // nearest reachable entry cell, otherwise the room centre, in subtiles
	int hops = 0;                    // rooms between the player and it, itself included
	bool complete = false; // no reachable uncovered floor/frontier remains (Nearest: no unvisited room)
	bool door = false; // approach an actual door for the player to open
};

// Floor coverage is separate from automap visibility and room entry. Only
// ground within ordinary discovery walking range of an actual player visit
// is covered, so a branch in an entered room remains a destination.
class Coverage {
public:
	void Observe(const LevelRoute::Memory& memory, const Navigation::Grid& grid);
	bool Seen(int x, int y) const;
	void Reject(Position position, uint64_t revision);
	bool Rejected(int x, int y, uint64_t revision) const;
private:
	int x_ = 0, y_ = 0, width_ = 0, height_ = 0;
	std::vector<uint8_t> cells_;
	std::vector<Position> rejected_;
	uint64_t revision_ = 0;
	Position rejectedFrom_;
};

// A floor target is finished once a real player observation covers it.
// Distance alone would finish a nearby target across a wall without seeing it.
// An unloaded door approach stops within two cells for the player's D instead.
bool Reached(const Coverage& coverage, Position player, Position target, bool doorApproach = false);

// Search walking floor and streaming frontiers, including inside entered
// rooms. Unknown collision never causes an entire room to be rejected.
Choice Next(const LevelRoute::Memory& memory, const Coverage& coverage, Position player, int bodyPattern);

// `explored` holds actual visits/current room (TerrainService::ExplorationRooms);
// `refused` holds the ids of rooms a walk has already given up on, so the next
// ask moves on instead of trying the same wall again. A border the grid proves
// closed is not walked through. Nothing is found when every room that can be
// reached is explored, or when the player stands in no room of the graph.
Choice Nearest(const RoomGraph::Graph& graph, const Navigation::Grid& grid,
	const std::vector<NativeRooms::Room>& explored, Position player,
	const std::vector<uint64_t>& refused = {}, const LevelRoute::Reach* reach = nullptr);
}
