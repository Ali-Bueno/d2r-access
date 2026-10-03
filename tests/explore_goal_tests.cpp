#include "explore_goal.h"
#include "test_support.h"

#include <vector>

using namespace D2RAccess;
using Radar::Position;

namespace {
NativeRooms::Room Room(int tileX, int tileY, int width = 8, int height = 8) {
	return {tileX, tileY, width, height, NativeRooms::Identity(tileX, tileY)};
}

// A grid whose cells are all unknown until a test says otherwise, so every
// border a test calls closed is one the test closed on purpose.
struct GridBuilder {
	Navigation::Grid grid;
	GridBuilder(int x, int y, int width, int height) {
		grid.x = x, grid.y = y, grid.width = width, grid.height = height;
		grid.cells.resize(static_cast<size_t>(width) * height);
	}
	void Set(int x0, int y0, int x1, int y1, Radar::State state) { // inclusive
		for (int y = y0; y <= y1; ++y)
			for (int x = x0; x <= x1; ++x)
				grid.cells[static_cast<size_t>(y - grid.y) * grid.width + (x - grid.x)].body = state;
	}
};
}

int main() {
	// A row of four rooms, A - B - C - D, each 8 tiles (40 subtiles) wide, and
	// one more, E, below A. The player stands in A.
	const std::vector<NativeRooms::Room> rooms{Room(0, 0), Room(8, 0), Room(16, 0), Room(24, 0), Room(0, 8)};
	const auto graph = RoomGraph::Build(rooms);
	const Navigation::Grid unread;
	const Position player{20, 20};
	{
		const auto choice = ExploreGoal::Nearest(graph, unread, {rooms[0]}, player);
		Check(choice.found && choice.hops == 1, "with only this room known, the next room over is chosen");
		Check(choice.room == 1 || choice.room == 4, "the choice is one of the rooms beside this one");
		Check(NativeRooms::Containing(graph.rooms, choice.position) == choice.room, "the place given lies inside the room chosen");
	}
	{
		// Standing near the bottom of A, E is the nearer of the two neighbours.
		const auto choice = ExploreGoal::Nearest(graph, unread, {rooms[0]}, {20, 38});
		Check(choice.found && choice.room == 4, "of two rooms one hop away, the nearer is chosen");
	}
	{
		const auto choice = ExploreGoal::Nearest(graph, unread, {rooms[0], rooms[1], rooms[4]}, player);
		Check(choice.found && choice.room == 2 && choice.hops == 2, "explored rooms are passed over for the first one that is not");
	}
	{
		const auto choice = ExploreGoal::Nearest(graph, unread, {rooms[0], rooms[4]}, player, {rooms[1].id});
		Check(choice.found && choice.room == 2, "a room already given up on is passed over");
	}
	{
		const auto choice = ExploreGoal::Nearest(graph, unread, rooms, player);
		Check(!choice.found, "a level with every room explored has nowhere new to go");
	}
	{
		const auto choice = ExploreGoal::Nearest(graph, unread, {rooms[0]}, {500, 500});
		Check(!choice.found, "a player standing in no room of the level is given nothing");
	}
	{
		// The border between A and B proved closed: B and everything past it can
		// only be reached some other way, and here there is none. E is left.
		GridBuilder builder(0, 0, 90, 80);
		builder.Set(0, 0, 89, 79, Radar::State::Open);
		builder.Set(39, 0, 40, 39, Radar::State::Blocked);
		const auto choice = ExploreGoal::Nearest(graph, builder.grid, {rooms[0]}, player);
		Check(choice.found && choice.room == 4, "a border the collision proves closed is not walked through");
		const auto none = ExploreGoal::Nearest(graph, builder.grid, {rooms[0], rooms[4]}, player);
		Check(!none.found, "rooms behind a closed border with no other way in are not offered");
	}
	Passed();
	return 0;
}
