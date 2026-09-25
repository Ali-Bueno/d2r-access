#include "room_graph.h"
#include "test_support.h"
#include <vector>

using namespace D2RAccess;
namespace {

NativeRooms::Room Room(int tileX, int tileY, int width = 8, int height = 8) {
	return {tileX, tileY, width, height, NativeRooms::Identity(tileX, tileY)};
}

// A grid whose cells are all unreachable until Open() says otherwise, so every
// reachable cell in a test is one the test put there on purpose.
struct GridBuilder {
	Navigation::Grid grid;
	GridBuilder(int x, int y, int width, int height) {
		grid.x = x, grid.y = y, grid.width = width, grid.height = height;
		grid.cells.resize(static_cast<size_t>(width) * height);
	}
	void Open(int x, int y, int steps) {
		const size_t at = static_cast<size_t>(y - grid.y) * grid.width + (x - grid.x);
		grid.cells[at].body = Radar::State::Open;
		grid.cells[at].steps = steps;
	}
};

}

int main() {
	// -- the native predicate: gap under six TILES on both axes ------------
	// Facing edges, negative where the rectangles overlap. RVA 0x3614C0.
	Check(RoomGraph::Near(Room(0, 0), Room(8, 0)), "rooms sharing an edge are near");
	Check(RoomGraph::Near(Room(0, 0), Room(13, 0)), "a five-tile gap is still under the six-tile threshold");
	Check(!RoomGraph::Near(Room(0, 0), Room(14, 0)), "a six-tile gap is not near: the test is strictly less than six");
	Check(!RoomGraph::Near(Room(0, 0), Room(0, 14)), "the threshold applies on the Y axis the same way");
	Check(!RoomGraph::Near(Room(0, 0), Room(8, 14)), "both axes must pass, not either");
	Check(RoomGraph::Near(Room(0, 0), Room(0, 0)), "the native rule holds for a room against itself");
	Check(RoomGraph::Near(Room(8, 0), Room(0, 0)) == RoomGraph::Near(Room(0, 0), Room(8, 0)), "nearness is symmetric whichever room asks");
	Check(RoomGraph::Near(Room(0, 0, 20, 20), Room(5, 5, 4, 4)), "an enclosed room overlaps, so its gap is negative and it is near");

	// -- touching: the only rooms one can walk between ----------------------
	Check(RoomGraph::Touching(Room(0, 0), Room(8, 0)), "rooms sharing an edge touch");
	Check(RoomGraph::Touching(Room(0, 0), Room(8, 4)), "a partly shared edge is still shared");
	Check(!RoomGraph::Touching(Room(0, 0), Room(8, 8)), "rooms meeting at a corner do not touch");
	Check(!RoomGraph::Touching(Room(0, 0), Room(13, 0)), "a gap the game still calls near is solid ground between");
	Check(RoomGraph::Touching(Room(0, 0, 20, 20), Room(5, 5, 4, 4)), "an enclosed room overlaps, so it touches");

	// -- adjacency ---------------------------------------------------------
	// A corridor: A - B - C in a row, D far away on its own, E on C's corner.
	const std::vector<NativeRooms::Room> rooms{Room(0, 0), Room(8, 0), Room(16, 0), Room(200, 200), Room(24, 8)};
	const auto graph = RoomGraph::Build(rooms);
	Check(graph.rooms.size() == 5 && graph.links.size() == 5, "the graph keeps one adjacency list per room");
	Check(graph.links[0].size() == 1 && graph.links[0][0] == 1, "a room links to its neighbour and not to itself");
	Check(graph.links[1].size() == 2, "a middle room links both ways");
	Check(graph.links[3].empty(), "an isolated room links to nothing");
	Check(graph.links[4].empty(), "a corner is not a way through");

	// -- room hops to the target ---------------------------------------------
	const auto hops = RoomGraph::Distances(graph, 2);
	Check(hops[2] == 0 && hops[1] == 1 && hops[0] == 2, "hops count the rooms between here and the target");
	Check(hops[3] == RoomGraph::NoRoute && hops[4] == RoomGraph::NoRoute, "no chain of touching rooms means no route");
	Check(RoomGraph::Distances(graph, 99)[0] == RoomGraph::NoRoute, "an out-of-range room cannot be routed to");

	// -- what the collision says about a shared edge ---------------------------
	// Room 0 spans subtiles [0,40), room 1 spans [40,80); the edge is x = 39|40.
	const auto wall = [](GridBuilder& builder, int x) {
		for (int y = 0; y < 40; ++y) builder.grid.cells[static_cast<size_t>(y) * builder.grid.width + x].body = Radar::State::Blocked;
	};
	{
		GridBuilder builder(0, 0, 90, 40);
		Check(RoomGraph::Border(builder.grid, rooms[0], rooms[1]) == RoomGraph::Passage::Unknown, "no collision proves nothing");
		builder.Open(39, 20, 5);
		builder.Open(40, 21, 6);
		Check(RoomGraph::Border(builder.grid, rooms[0], rooms[1]) == RoomGraph::Passage::Open, "a diagonal step across the edge is a passage");
	}
	{
		GridBuilder builder(0, 0, 90, 40);
		wall(builder, 39);
		Check(RoomGraph::Border(builder.grid, rooms[0], rooms[1]) == RoomGraph::Passage::Closed,
			"a known wall along one side closes the edge whatever lies beyond");
		Check(RoomGraph::Distances(graph, 2, &builder.grid)[0] == RoomGraph::NoRoute, "a closed edge is not a route");
	}
	{
		GridBuilder builder(0, 0, 90, 40);
		wall(builder, 39);
		wall(builder, 40);
		builder.Open(39, 5, 5);
		builder.Open(40, 30, 6);
		Check(RoomGraph::Border(builder.grid, rooms[0], rooms[1]) == RoomGraph::Passage::Closed,
			"open cells on both sides that never meet are no passage");
	}
	Check(RoomGraph::Border(GridBuilder(0, 0, 90, 40).grid, rooms[0], rooms[2]) == RoomGraph::Passage::Unknown,
		"rooms without a shared edge have no border to judge");

	// -- the sub-goal is always a cell the grid proved reachable -----------
	// Only one cell of room 1 is reachable cheaply: the doorway.
	{
		GridBuilder builder(0, 0, 90, 40);
		builder.Open(35, 20, 10); // in room 0, beside the boundary
		builder.Open(41, 20, 11); // in room 1: the only way in
		builder.Open(70, 5, 99);  // in room 1 but far round the back
		const auto goal = RoomGraph::ChooseSubGoal(graph, builder.grid, 0, 2);
		Check(goal.known && goal.inNextRoom, "the sub-goal lands in a nearer room when that room has collision");
		Check(goal.room == 1, "the sub-goal reports which room it is in");
		Check(goal.position.x == 41 && goal.position.y == 20, "the cheapest reachable cell is the doorway, not the far side of the room");
		Check(goal.steps == 11, "the sub-goal carries the step count the grid proved");
	}
	// The grid reaches a room nearer still: skip ahead to it.
	{
		GridBuilder builder(0, 0, 90, 40);
		builder.Open(41, 20, 11);
		builder.Open(81, 20, 50); // in room 2, the target room
		const auto goal = RoomGraph::ChooseSubGoal(graph, builder.grid, 0, 2);
		Check(goal.known && goal.room == 2 && goal.position.x == 81, "the room fewest hops from the target wins over a cheaper one");
	}
	// Nothing in the next room is reachable and it has no collision yet: walk
	// to the boundary on this side, which is what loads it.
	{
		GridBuilder builder(0, 0, 90, 40);
		builder.Open(5, 20, 1);   // deep inside room 0
		builder.Open(38, 20, 8);  // inside room 0, hard against the boundary
		const auto goal = RoomGraph::ChooseSubGoal(graph, builder.grid, 0, 2);
		Check(goal.known && !goal.inNextRoom, "with no collision in the next room the sub-goal stays in this one");
		Check(goal.room == 0 && goal.position.x == 38, "it is the reachable cell closest to the next room's rectangle");
	}
	// The next room is loaded and none of it is reachable: a known wall, not a way on.
	{
		GridBuilder builder(0, 0, 90, 40);
		builder.Open(38, 20, 8);
		builder.grid.cells[20 * 90 + 60].body = Radar::State::Blocked; // room 1 has collision
		Check(!RoomGraph::ChooseSubGoal(graph, builder.grid, 0, 2).known,
			"never lead to the edge of a loaded room the grid cannot reach");
	}
	// A blocked room with no reachable cell anywhere yields nothing at all.
	{
		GridBuilder builder(0, 0, 90, 40);
		Check(!RoomGraph::ChooseSubGoal(graph, builder.grid, 0, 1).known, "no reachable cell means no sub-goal, never a guessed heading");
	}
	// Nowhere nearer to go.
	{
		GridBuilder builder(0, 0, 90, 40);
		builder.Open(5, 20, 1);
		Check(!RoomGraph::ChooseSubGoal(graph, builder.grid, 0, 0).known, "the target's own room has no nearer room");
		Check(!RoomGraph::ChooseSubGoal(graph, builder.grid, 0, 3).known, "an unconnected target has no sub-goal");
	}
	// The next room lies outside the collision window entirely.
	{
		GridBuilder builder(0, 0, 30, 30); // covers only part of room 0
		builder.Open(25, 20, 4);
		const auto goal = RoomGraph::ChooseSubGoal(graph, builder.grid, 0, 1);
		Check(goal.known && !goal.inNextRoom && goal.position.x == 25, "a next room outside the window degrades to this room's boundary cell");
	}
	Passed();
}
