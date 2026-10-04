#include "explore_goal.h"
#include "navigation_policy.h"
#include "test_support.h"
#include "fixtures/spider_forest_routing.h"

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
	{
		// A dock runs along the near edge of the next room, with water at its
		// centre. Navigation has proved the dock reachable from this room.
		GridBuilder dock(0, 0, 80, 40);
		dock.Set(0, 0, 79, 39, Radar::State::Blocked);
		dock.Set(0, 18, 44, 22, Radar::State::Open);
		for (int x = 40; x <= 44; ++x) dock.grid.cells[static_cast<size_t>(20) * 80 + x].steps = x - 20;
		const std::vector<NativeRooms::Room> town{rooms[0], rooms[1]};
		const auto townGraph = RoomGraph::Build(town);
		const auto oldKnown = NavigationPolicy::Explored({rooms[0]}, &town, town, player);
		Check(!ExploreGoal::Nearest(townGraph, dock.grid, oldKnown, player).found,
			"fixture: charted town scenery exhausted the old exploration goals");
		const auto known = NavigationPolicy::ForExploration(75, {rooms[0]}, &town, town, player);
		const auto choice = ExploreGoal::Nearest(townGraph, dock.grid, known, player);
		Check(choice.found && choice.room == 1 && choice.position.x == 40 && choice.position.y == 20,
			"exploration enters reachable dock floor instead of aiming at a blocked room centre");
		Check(NativeRooms::Containing({rooms[0], rooms[1]}, choice.position) == 1,
			"reachable entry target still lies inside the unexplored room");
	}
	{
		// One hop toward unseen water is geometrically closer; a two-hop entry
		// has a complete walking route. H must choose the actual floor first.
		GridBuilder known(0, 0, 120, 80);
		known.Set(0, 0, 119, 39, Radar::State::Open);
		known.grid.cells[20 * 120 + 80].steps = 60;
		const auto choice = ExploreGoal::Nearest(graph, known.grid, {rooms[0], rooms[1], rooms[3]}, player);
		Check(choice.found && choice.room == 2 && choice.position.x == 80,
			"reachable entry wins over a nearer speculative room");
		known.grid.cells[40 * 120 + 20].body = Radar::State::Open;
		known.grid.cells[40 * 120 + 20].steps = 100;
		const auto nearer = ExploreGoal::Nearest(graph, known.grid, {rooms[0], rooms[1], rooms[3]}, player);
		Check(nearer.found && nearer.room == 2,
			"actual walking steps rank reachable rooms ahead of geometric hops");
		const auto next = ExploreGoal::Nearest(graph, known.grid, {rooms[0], rooms[1], rooms[3]}, player, {rooms[2].id});
		Check(next.found && next.room == 4, "a newly unavailable goal has another reachable room to choose");
	}
	{
		// User's read-only 2026-10-04 Spider Forest collision capture, including
		// actual loaded grids, full logical layout and the matching stored visits.
		const auto captured = SpiderForestRouting::Capture();
		const auto layout = RoomGraph::Build(captured.rooms);
		const auto grid = Navigation::Build(captured.world, Navigation::MaxRadius, Radar::Layer::Terrain, false);
		const auto visits = NavigationPolicy::ForExploration(76, captured.visited, &captured.rooms, captured.rooms, captured.world.player);
		const auto choice = ExploreGoal::Nearest(layout, grid, visits, captured.world.player);
		Check(choice.found && layout.rooms[choice.room].tileX == 1016 && layout.rooms[choice.room].tileY == 912,
			"captured Spider Forest chooses the nearest reachable unvisited floor despite charted scenery");
		const auto* entry = grid.At(static_cast<int>(choice.position.x), static_cast<int>(choice.position.y));
		Check(entry && entry->steps == 72 && entry->body == Radar::State::Open,
			"captured Spider Forest entry has a complete 72-step native-collision route");
	}
	Passed();
	return 0;
}
