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

	// -- adjacency ---------------------------------------------------------
	// A corridor: A - B - C in a row, plus D far away on its own.
	const std::vector<NativeRooms::Room> rooms{Room(0, 0), Room(8, 0), Room(16, 0), Room(200, 200)};
	const auto graph = RoomGraph::Build(rooms);
	Check(graph.rooms.size() == 4 && graph.near.size() == 4, "the graph keeps one adjacency list per room");
	Check(graph.near[0].size() == 1 && graph.near[0][0] == 1, "a room links to its neighbour and not to itself");
	Check(graph.near[1].size() == 2, "a middle room links both ways");
	Check(graph.near[3].empty(), "an isolated room links to nothing");

	// -- routing -----------------------------------------------------------
	const auto route = RoomGraph::Route(graph, 0, 2);
	Check(route.size() == 3 && route[0] == 0 && route[1] == 1 && route[2] == 2, "the route is inclusive of both ends and passes through the middle");
	Check(RoomGraph::Route(graph, 2, 2).size() == 1, "a route to the room you are in is just that room");
	Check(RoomGraph::Route(graph, 0, 3).empty(), "no chain of near rooms means no route, not a straight line");
	Check(RoomGraph::Route(graph, 0, 99).empty(), "an out-of-range room cannot be routed to");
	Check(RoomGraph::Route(RoomGraph::Build({}), 0, 0).empty(), "an empty level has no routes");

	// -- the sub-goal is always a cell the grid proved reachable -----------
	// Room 0 spans subtiles [0,40), room 1 spans [40,80). The grid covers
	// both. Only one cell of room 1 is reachable: the doorway.
	{
		GridBuilder builder(0, 0, 90, 40);
		builder.Open(35, 20, 10); // in room 0, beside the boundary
		builder.Open(41, 20, 11); // in room 1: the only way in
		builder.Open(70, 5, 99);  // in room 1 but far round the back
		const auto goal = RoomGraph::ChooseSubGoal(graph, builder.grid, {0, 1, 2});
		Check(goal.known && goal.inNextRoom, "the sub-goal lands in the next room when that room has collision");
		Check(goal.room == 1, "the sub-goal reports which room it is in");
		Check(goal.position.x == 41 && goal.position.y == 20, "the cheapest reachable cell is the doorway, not the far side of the room");
		Check(goal.steps == 11, "the sub-goal carries the step count the grid proved");
	}
	// Nothing in the next room is reachable: fall back to the boundary on
	// this side rather than inventing a point.
	{
		GridBuilder builder(0, 0, 90, 40);
		builder.Open(5, 20, 1);   // deep inside room 0
		builder.Open(38, 20, 8);  // inside room 0, hard against the boundary
		const auto goal = RoomGraph::ChooseSubGoal(graph, builder.grid, {0, 1});
		Check(goal.known && !goal.inNextRoom, "with no collision in the next room the sub-goal stays in this one");
		Check(goal.room == 0 && goal.position.x == 38, "it is the reachable cell closest to the next room's rectangle");
	}
	// A blocked room with no reachable cell anywhere yields nothing at all.
	{
		GridBuilder builder(0, 0, 90, 40);
		const auto goal = RoomGraph::ChooseSubGoal(graph, builder.grid, {0, 1});
		Check(!goal.known, "no reachable cell means no sub-goal, never a guessed heading");
	}
	// A route with nowhere to go next.
	{
		GridBuilder builder(0, 0, 90, 40);
		builder.Open(5, 20, 1);
		Check(!RoomGraph::ChooseSubGoal(graph, builder.grid, {0}).known, "a one-room route has no next room");
		Check(!RoomGraph::ChooseSubGoal(graph, builder.grid, {}).known, "an empty route has no sub-goal");
	}
	// The next room lies outside the collision window entirely.
	{
		GridBuilder builder(0, 0, 30, 30); // covers only part of room 0
		builder.Open(25, 20, 4);
		const auto goal = RoomGraph::ChooseSubGoal(graph, builder.grid, {0, 1});
		Check(goal.known && !goal.inNextRoom && goal.position.x == 25, "a next room outside the window degrades to this room's boundary cell");
	}
	Passed();
}
