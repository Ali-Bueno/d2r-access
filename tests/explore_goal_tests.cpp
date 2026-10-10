#include "explore_goal.h"
#include "auto_walk.h"
#include <optional>
#include <algorithm>
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
	{
		// A closed door is a currently blocked local border, but not proof the
		// ground behind it cannot be explored. The remembered reach gate allows
		// doors; its answer must not be vetoed by a second cropped-border gate.
		const std::vector<NativeRooms::Room> partition{rooms[0], rooms[1]};
		const auto partitionGraph = RoomGraph::Build(partition);
		Radar::World world;
		world.session = 1; world.level = 79; world.player = {20, 20};
		world.bodyPattern = 1; world.walkingMask = 0x1C09;
		world.rooms.push_back({0, 0, 80, 40, std::vector<uint16_t>(80 * 40, 0)});
		for (int y = 0; y < 40; ++y) world.rooms[0].cells[y * 80 + 39] = Radar::Collision::Door;
		const auto grid = Navigation::Build(world, 100, Radar::Layer::Terrain, false);
		LevelRoute::Memory memory;
		Check(memory.Begin(1, 79, partition) && memory.Learn(world), "door-partition exploration regression learns floor");
		const LevelRoute::Reach reach(memory, world.player, world.bodyPattern);
		const auto old = ExploreGoal::Nearest(partitionGraph, grid, {rooms[0]}, world.player);
		Check(!old.found && !old.complete, "local closed-border filtering reproduces the remaining-ground exclusion");
		const auto choice = ExploreGoal::Nearest(partitionGraph, grid, {rooms[0]}, world.player, {}, &reach);
		Check(choice.found && choice.room == 1 && !choice.complete,
			"H keeps an unexplored destination allowed by the shared whole-level reach policy");
		const auto refused = ExploreGoal::Nearest(partitionGraph, grid, {rooms[0]}, world.player, {rooms[1].id}, &reach);
		Check(!refused.found && !refused.complete, "giving up on a goal cannot claim all ground is explored");
		const auto done = ExploreGoal::Nearest(partitionGraph, grid, partition, world.player, {}, &reach);
		Check(!done.found && done.complete, "completion is distinct from a missing route to remaining rooms");
		for (int y = 0; y < 40; ++y) world.rooms[0].cells[y * 80 + 39] = Radar::Collision::Wall;
		(void)memory.Learn(world);
		const LevelRoute::Reach walled(memory, world.player, world.bodyPattern);
		Check(!ExploreGoal::Nearest(partitionGraph, grid, {rooms[0]}, world.player, {}, &walled).found,
			"a genuine wall cut-off remains excluded even when the native room graph is connected");
	}
	{
		// Every native room is already visited, but long dock branches and town
		// interiors remain outside the player's discovery range. H must sweep
		// the actual floor, regardless of act or archived room visitation.
		for (const int level : {1, 40, 75, 103, 109}) {
			Radar::World world;
			world.session = 1; world.level = level; world.player = {10, 20}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
			world.rooms.push_back({0, 0, 120, 80, std::vector<uint16_t>(120 * 80, Radar::Collision::Wall)});
			const auto open = [&](int x0, int y0, int x1, int y1) {
				for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) world.rooms[0].cells[y * 120 + x] = 0;
			};
			open(5, 18, 110, 22); open(78, 18, 82, 65); open(40, 60, 82, 65);
			const std::vector<NativeRooms::Room> shape{{0, 0, 24, 16, 1}};
			LevelRoute::Memory memory; memory.Begin(1, level, shape); memory.Learn(world);
			ExploreGoal::Coverage coverage;
			bool finished = false;
			AutoWalk::Walker walker;
			AutoWalk::Settings settings;
			settings.layer = Radar::Layer::Movement;
			std::optional<Position> target;
			int legs = 0;
			// One exploration run: consume actual walking orders around the dock
			// bends instead of teleporting directly to each selected floor point.
			for (int tick = 0; tick < 250; ++tick) {
				const auto grid = Navigation::Build(world, 150, Radar::Layer::Terrain, false);
				coverage.Observe(memory, grid);
				if (!target || ExploreGoal::Reached(coverage, world.player, *target)) {
					const auto next = ExploreGoal::Next(memory, coverage, world.player, world.bodyPattern);
					if (!next.found) { finished = next.complete; walker.Stop(); break; }
					Check(!next.door, "open dock branches need no manual door interaction");
					target = next.position;
					++legs;
					walker.Start();
				}
				const auto route = LevelRoute::Plan().From(memory, world.player, *target, 0, world.bodyPattern);
				Check(!route.path.empty(), "each successive exploration target has a proved walking route");
				const auto step = walker.Tick(world, route.path, 1000 + tick * 300, settings);
				Check(step.event == AutoWalk::Event::Order && AutoWalk::Straight(world, world.player, step.order, settings.layer),
					"continuous exploration orders follow actual floor around bends without crossing water");
				world.player = step.order;
			}
			Check(legs > 1 && !walker.Active(), "one run chains multiple floor targets and stops when exploration is complete");
			Check(finished && coverage.Seen(105, 20) && coverage.Seen(45, 62),
				"the same floor sweep finishes both dock branches in Acts 1 through 5 without targeting surrounding water");
		}
	}
	{
		const auto captured = SpiderForestRouting::Capture();
		LevelRoute::Memory memory;
		Check(memory.Begin(captured.world.session, captured.world.level, captured.rooms) && memory.Learn(captured.world),
			"actual Spider Forest layout and native collision initialize floor exploration");
		const auto grid = Navigation::Build(captured.world, 150, Radar::Layer::Terrain, false);
		ExploreGoal::Coverage coverage; coverage.Observe(memory, grid);
		const auto next = ExploreGoal::Next(memory, coverage, captured.world.player, captured.world.bodyPattern);
		const auto* floor = grid.At(static_cast<int>(next.position.x), static_cast<int>(next.position.y));
		Check(next.found && floor && floor->body == Radar::State::Open && floor->steps > 0
			&& !coverage.Seen(static_cast<int>(next.position.x), static_cast<int>(next.position.y)),
			"captured Act 3 data yields a complete route to new actual floor inside rooms the old room-entry policy discards");
		LevelRoute::Reach reach(memory, captured.world.player, captured.world.bodyPattern);
		for (const auto& room : captured.rooms) {
			const auto bounds = NativeRooms::Subtiles(room);
			if (reach.Within(bounds) != LevelRoute::Reach::Answer::CutOff) continue;
			bool open = false;
			for (int y = bounds.minY; y < bounds.maxY; ++y) for (int x = bounds.minX; x < bounds.maxX; ++x)
				open = open || memory.At(x, y) == LevelRoute::Ground::Open;
			Check(!open, "the captured rejected room rectangles contain no measured walking floor, rather than unexplored content");
		}
	}
	{
		const std::vector<NativeRooms::Room> shape{{0, 0, 8, 8, 1}, {8, 0, 8, 8, 2}, {16, 0, 8, 8, 3}};
		Radar::World world;
		world.session = 1; world.level = 76; world.player = {10, 20}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
		LevelRoute::Memory memory; memory.Begin(1, 76, shape);
		ExploreGoal::Coverage coverage;
		bool finished = false;
		for (int tap = 0; tap < 80; ++tap) {
			const int room = std::clamp(static_cast<int>(world.player.x) / 40, 0, 2);
			world.rooms.clear();
			for (int loaded = room; loaded <= std::min(2, room + 1); ++loaded) {
				world.rooms.push_back({loaded * 40, 0, 40, 40, std::vector<uint16_t>(1600, Radar::Collision::Wall)});
				for (int y = 18; y <= 22; ++y) for (int x = 0; x < 40; ++x) world.rooms.back().cells[y * 40 + x] = 0;
			}
			memory.Learn(world);
			const auto grid = Navigation::Build(world, 150, Radar::Layer::Terrain, false);
			coverage.Observe(memory, grid);
			const auto next = ExploreGoal::Next(memory, coverage, world.player, world.bodyPattern);
			if (!next.found) { finished = next.complete; break; }
			const auto route = LevelRoute::Plan().From(memory, world.player, next.position, 0, 1);
			Check(!route.path.empty(), "streamed exploration retains a full route after preceding room collision unloads");
			world.player = route.path.front();
			for (const auto point : route.path) {
				if (memory.At(static_cast<int>(point.x), static_cast<int>(point.y)) != LevelRoute::Ground::Open) break;
				world.player = point;
			}
		}
		Check(finished && coverage.Seen(112, 20), "exploration follows streaming frontiers through all rooms and finishes their actual walking floor");
	}
	{
		Radar::World world;
		world.session = 1; world.level = 109; world.player = {10, 20}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
		world.rooms = {{0, 0, 80, 40, std::vector<uint16_t>(3200, Radar::Collision::Wall)}};
		for (int y = 18; y <= 22; ++y) for (int x = 5; x <= 70; ++x)
			world.rooms[0].cells[y * 80 + x] = x == 39 ? Radar::Collision::Door : 0;
		const std::vector<NativeRooms::Room> shape{{0, 0, 16, 8, 1}};
		LevelRoute::Memory memory; memory.Begin(1, 109, shape); memory.Learn(world);
		ExploreGoal::Coverage coverage;
		world.player = {25, 20};
		coverage.Observe(memory, Navigation::Build(world, 100, Radar::Layer::Terrain, false));
		world.player = {10, 20};
		coverage.Observe(memory, Navigation::Build(world, 100, Radar::Layer::Terrain, false));
		const auto door = ExploreGoal::Next(memory, coverage, world.player, 1);
		Check(door.found && door.door && door.position.x == 39,
			"an unexplored branch through a closed door produces a door approach, not a refusal or false completion");
		for (int y = 18; y <= 22; ++y) world.rooms[0].cells[y * 80 + 39] = 0;
		memory.Learn(world);
		const auto opened = ExploreGoal::Next(memory, coverage, world.player, 1);
		Check(opened.found && !opened.door, "opening the door makes its unexplored floor an ordinary walking destination");
		coverage.Reject(opened.position, memory.Revision());
		const auto another = ExploreGoal::Next(memory, coverage, world.player, 1);
		Check(another.found && !another.complete, "one refused point never discards its room or declares that unexplored content is exhausted");
	}
	{
		Radar::World world;
		world.session = 1; world.level = 76; world.player = {25, 20}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
		world.rooms = {{0, 0, 40, 40, std::vector<uint16_t>(1600, Radar::Collision::Wall)}};
		for (int y = 18; y <= 22; ++y) for (int x = 5; x < 40; ++x) world.rooms[0].cells[y * 40 + x] = 0;
		LevelRoute::Memory memory; memory.Begin(1, 76, {{0, 0, 16, 8, 1}}); memory.Learn(world);
		ExploreGoal::Coverage coverage;
		coverage.Observe(memory, Navigation::Build(world, 100, Radar::Layer::Terrain, false));
		world.player = {10, 20};
		coverage.Observe(memory, Navigation::Build(world, 100, Radar::Layer::Terrain, false));
		const auto floor = ExploreGoal::Next(memory, coverage, world.player, 1);
		Check(floor.found && memory.At(static_cast<int>(floor.position.x), static_cast<int>(floor.position.y)) == LevelRoute::Ground::Open,
			"partially streamed branch first chooses its remaining measured floor");
		coverage.Reject(floor.position, memory.Revision());
		const auto frontier = ExploreGoal::Next(memory, coverage, world.player, 1);
		Check(frontier.found && memory.At(static_cast<int>(frontier.position.x), static_cast<int>(frontier.position.y)) == LevelRoute::Ground::Unseen,
			"a failed measured point cannot prevent exploration of an unloaded branch");
	}
	{
		// A thin wall separates two nearby cells. The remaining floor is close
		// in coordinates but requires walking around the wall's lower end.
		Radar::World world;
		world.session = 1; world.level = 75; world.player = {18, 20}; world.bodyPattern = 0; world.walkingMask = 0x1C09;
		world.rooms = {{0, 0, 40, 40, std::vector<uint16_t>(1600, 0)}};
		for (int y = 0; y <= 25; ++y) world.rooms[0].cells[y * 40 + 19] = Radar::Collision::Wall;
		LevelRoute::Memory memory; memory.Begin(1, 75, {Room(0, 0)}); memory.Learn(world);
		ExploreGoal::Coverage coverage;
		coverage.Observe(memory, Navigation::Build(world, 16, Radar::Layer::Terrain, false));
		Check(!ExploreGoal::Reached(coverage, world.player, {20, 20}),
			"a floor target two cells away across a wall stays active until actually observed");
		Check(ExploreGoal::Reached(coverage, world.player, {17, 20}),
			"already observed floor completes its leg without forcing another movement order");
		Check(ExploreGoal::Reached(coverage, world.player, {20, 20}, true),
			"an explicit door approach retains its nearby stop for manual interaction");
		world.player = {20, 20};
		coverage.Observe(memory, Navigation::Build(world, 16, Radar::Layer::Terrain, false));
		Check(ExploreGoal::Reached(coverage, world.player, {20, 20}),
			"walking around the wall observes the target and enables automatic continuation");
	}
	Passed();
	return 0;
}
