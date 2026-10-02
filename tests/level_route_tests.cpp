#include "beacon.h"
#include "level_route.h"
#include "navigation_grid.h"
#include "test_support.h"

#include <cmath>
#include <vector>

using namespace D2RAccess;
using LevelRoute::Ground;
using Radar::Position;
using Radar::World;

namespace {
constexpr uint16_t PlayerPath = 0x1C09; // observed native player walking mask
constexpr uint16_t Wall = 0x0001, Blank = 0x0020, Monster = 0x0100, NoPath = 0x1000;
constexpr int RoomTiles = 8, RoomCells = RoomTiles * 5;
// A level of Across x Down rooms whose first tile is not the map's origin.
constexpr int Across = 5, Down = 5, TileX = 100, TileY = 100;
constexpr int Left = TileX * 5, Top = TileY * 5, Width = Across * RoomCells, Height = Down * RoomCells;

struct Level {
	std::vector<uint16_t> cells = std::vector<uint16_t>(static_cast<size_t>(Width) * Height, 0);
	uint16_t& At(int x, int y) { return cells[static_cast<size_t>(y - Top) * Width + (x - Left)]; }
	void Fill(int x0, int y0, int x1, int y1, uint16_t flags) { // inclusive, level cells
		for (int y = y0; y <= y1; ++y)
			for (int x = x0; x <= x1; ++x) At(x, y) = flags;
	}
	std::vector<NativeRooms::Room> Rooms() const {
		std::vector<NativeRooms::Room> rooms;
		for (int row = 0; row < Down; ++row)
			for (int column = 0; column < Across; ++column) {
				const int x = TileX + column * RoomTiles, y = TileY + row * RoomTiles;
				rooms.push_back({x, y, RoomTiles, RoomTiles, NativeRooms::Identity(x, y)});
			}
		return rooms;
	}
	// What the game would have loaded: the rooms within `reach` rooms of the player's.
	World Loaded(Position player, int reach, int pattern = 1) const {
		World world;
		world.session = world.level = 1;
		world.player = player;
		world.bodyPattern = pattern;
		world.walkingMask = PlayerPath;
		const int column = (static_cast<int>(player.x) - Left) / RoomCells, row = (static_cast<int>(player.y) - Top) / RoomCells;
		for (int r = std::max(0, row - reach); r <= std::min(Down - 1, row + reach); ++r)
			for (int c = std::max(0, column - reach); c <= std::min(Across - 1, column + reach); ++c) {
				Radar::RoomGrid room{Left + c * RoomCells, Top + r * RoomCells, RoomCells, RoomCells, {}};
				for (int y = 0; y < RoomCells; ++y)
					for (int x = 0; x < RoomCells; ++x)
						room.cells.push_back(cells[static_cast<size_t>(room.y - Top + y) * Width + (room.x - Left + x)]);
				world.rooms.push_back(std::move(room));
			}
		return world;
	}
};

double Distance(Position a, Position b) { return std::hypot(a.x - b.x, a.y - b.y); }
bool Reached(const Navigation::Grid& grid, Position place, int margin) {
	for (int dy = -margin; dy <= margin; ++dy)
		for (int dx = -margin; dx <= margin; ++dx) {
			const auto* cell = grid.At(static_cast<int>(place.x) + dx, static_cast<int>(place.y) + dy);
			if (cell && cell->steps >= 0) return true;
		}
	return false;
}
}

int main() {
	{
		LevelRoute::Memory memory;
		Check(!memory.Begin(1, 1, {}), "a level with no rooms is not kept");
		Check(memory.Revision() == 0 && memory.At(Left, Top) == Ground::Outside, "nothing is kept of it");
		Level level;
		Check(memory.Begin(1, 1, level.Rooms()), "a level's rooms begin its map");
		Check(memory.At(Left, Top) == Ground::Unseen && memory.At(Left + Width - 1, Top + Height - 1) == Ground::Unseen,
			"ground inside a room is unseen until the game has loaded it");
		Check(memory.At(Left - 1, Top) == Ground::Outside && memory.At(Left, Top + Height) == Ground::Outside,
			"ground outside every room is no part of the level");

		// What is learnt: walls, and none of what a unit leaves on the floor.
		level.Fill(Left + 10, Top + 10, Left + 12, Top + 10, Wall);
		level.At(Left + 20, Top + 10) = NoPath;           // a marker left behind
		level.At(Left + 22, Top + 10) = Monster | NoPath; // a monster standing there
		level.At(Left + 24, Top + 10) = Blank;
		const World first = level.Loaded({Left + 5.0, Top + 5.0}, 0);
		Check(memory.Learn(first), "the first look at a room changes the map");
		const auto revision = memory.Revision();
		Check(!memory.Learn(first) && memory.Revision() == revision, "the same look again changes nothing");
		Check(memory.At(Left + 11, Top + 10) == Ground::Blocked, "a wall is ground seen shut");
		Check(memory.At(Left + 20, Top + 10) == Ground::Open, "a marker left on the floor is not remembered as a wall");
		Check(memory.At(Left + 22, Top + 10) == Ground::Open, "a monster is not remembered as a wall");
		Check(memory.At(Left + 24, Top + 10) == Ground::Blocked, "a blank cell of a loaded room is no floor");
		Check(memory.At(Left + RoomCells, Top) == Ground::Unseen, "a room not loaded stays unseen");
		level.At(Left + 30, Top + 30) = Wall;
		World changed = level.Loaded({Left + 5.0, Top + 5.0}, 0);
		changed.level = 2;
		Check(!memory.Learn(changed) && memory.At(Left + 30, Top + 30) == Ground::Open, "another level's rooms are not learnt");
		Check(memory.Learn(level.Loaded({Left + 5.0, Top + 5.0}, 0)) && memory.Revision() == revision + 1,
			"a cell that changed is learnt, and counted once");
	}
	{
		// The body: a five-cell cross needs three cells of width, a point one.
		Level level;
		level.Fill(Left, Top + 20, Left + Width - 1, Top + 20, Wall);
		level.Fill(Left + 50, Top + 20, Left + 51, Top + 20, 0);  // two cells wide
		LevelRoute::Memory memory;
		Check(memory.Begin(1, 1, level.Rooms()), "fixture: the map begins");
		for (int row = 0; row < Down; ++row)
			for (int column = 0; column < Across; ++column)
				(void)memory.Learn(level.Loaded({Left + column * RoomCells + 5.0, Top + row * RoomCells + 5.0}, 0));
		const Position from{Left + 50.0, Top + 60.0}, place{Left + 50.0, Top + 5.0};
		Check(LevelRoute::Field(memory, place, 0, 1, true).From(from).empty(), "a cross body does not pass a gap two cells wide");
		Check(!LevelRoute::Field(memory, place, 0, 0, true).From(from).empty(), "a point body does");
		level.Fill(Left + 120, Top + 20, Left + 122, Top + 20, 0); // three cells wide, far along the wall
		for (int column = 0; column < Across; ++column) (void)memory.Learn(level.Loaded({Left + column * RoomCells + 5.0, Top + 5.0}, 0));
		const LevelRoute::Field field(memory, place, 0, 1, false);
		const auto path = field.From(from);
		Check(!path.empty() && path.back().x == place.x && path.back().y == place.y, "a way ends at the place");
		bool through = false, adjacent = true;
		Position previous = from;
		for (const auto point : path) {
			if (point.y == Top + 20) through = through || point.x == Left + 121;
			adjacent = adjacent && std::abs(point.x - previous.x) <= 1 && std::abs(point.y - previous.y) <= 1;
			Check(memory.At(static_cast<int>(point.x), static_cast<int>(point.y)) == Ground::Open, "a way crosses only open ground");
			previous = point;
		}
		Check(through, "it passes the wall by the middle of the gap three cells wide");
		Check(adjacent, "each cell of a way is beside the last");
		Check(field.At(place) && !field.At(from) && field.From(place).empty(), "at the place there is nothing left to walk");
		// Standing where the body does not fit, as beside a wall the game let the
		// character reach: the way starts from the cell beside it.
		const Position against{Left + 60.0, Top + 21.0};
		Check(!field.From(against).empty(), "a cell the body does not fit on can still be left");
		Check(LevelRoute::Field(memory, {Left - 50.0, Top + 5.0}, 1, 1, true).From(from).empty(), "a place outside the level has no way");
	}
	{
		// A way all seen is taken before a shorter one that is part guess.
		Level level;
		level.Fill(Left, Top + 100, Left + Width - 1, Top + 102, Wall);
		level.Fill(Left + 4, Top + 100, Left + 8, Top + 102, 0); // the only gap, at the far left
		LevelRoute::Memory memory;
		Check(memory.Begin(1, 1, level.Rooms()), "fixture: the map begins");
		const Position from{Left + 180.0, Top + 140.0}, place{Left + 180.0, Top + 60.0};
		LevelRoute::Plan plan;
		// Nothing seen: straight at it.
		auto way = plan.From(memory, from, place, 1, 1);
		Check(!way.path.empty() && way.guess, "a place on ground not seen yet is walked toward as a guess");
		Check(way.path.size() <= 80, "and straight, with nothing known to be in the way");
		// The rows by the wall seen, right to left; the middle column of the top rows never.
		for (int row = 1; row <= 3; ++row)
			for (int column = 0; column < Across; ++column)
				(void)memory.Learn(level.Loaded({Left + column * RoomCells + 5.0, Top + row * RoomCells + 5.0}, 0));
		way = plan.From(memory, from, place, 1, 1);
		Check(!way.path.empty() && !way.guess, "with the wall and its gap seen, the way is one all seen");
		Check(way.path.size() > 300, "and goes round by the gap");
		bool crossed = false;
		for (const auto point : way.path)
			if (point.y >= Top + 100 && point.y <= Top + 102) crossed = crossed || (point.x >= Left + 4 && point.x <= Left + 8);
		Check(crossed, "through the gap itself");
		// Seen shut all round: no way, seen or guessed.
		level.Fill(Left + 4, Top + 100, Left + 8, Top + 102, Wall);
		for (int column = 0; column < Across; ++column) (void)memory.Learn(level.Loaded({Left + column * RoomCells + 5.0, Top + 2 * RoomCells + 5.0}, 0));
		way = plan.From(memory, from, place, 1, 1);
		Check(way.path.empty() && !way.guess, "ground seen shut is never crossed, even as a guess");
	}
	// The Far Oasis, 2026-10-01, as a fixture. A ridge shuts a pocket on three
	// sides; the place lies beyond its closed end, and the way to it is back
	// out of the open end and round. The game keeps only the rooms near the
	// player's loaded: one room each way, or two. The walk goes in once, sees
	// the ridge, and must come out and round without turning back in when the
	// ridge is freed again. With two rooms each way the routing before this,
	// the loaded floor's own route by the ridge and the room graph's guess past
	// it, turned round at the room border for ever (292 turns in 300 looks,
	// run offline 2026-10-01); with one it walked to the ridge and stopped.
	for (const int reach : {1, 2}) {
		Level level;
		level.Fill(Left + 60, Top + 60, Left + 140, Top + 62, Wall);   // the closed end
		level.Fill(Left + 60, Top + 60, Left + 62, Top + 150, Wall);   // one side
		level.Fill(Left + 138, Top + 60, Left + 140, Top + 150, Wall); // the other
		const Position place{Left + 100.0, Top + 20.0};
		Position player{Left + 100.0, Top + 180.0};
		LevelRoute::Memory memory;
		LevelRoute::Plan plan;
		Check(memory.Begin(1, 1, level.Rooms()), "fixture: the map begins");
		const Beacon::Settings settings;
		const int margin = 1;
		bool arrived = false, led = false;
		int ticks = 0, entered = 0;
		bool inside = false;
		for (; ticks < 400 && !arrived; ++ticks) {
			const World world = level.Loaded(player, reach);
			(void)memory.Learn(world);
			const auto grid = Navigation::Build(world, Navigation::MaxRadius, Radar::Layer::Terrain, false);
			Position destination = place;
			if (!Reached(grid, place, margin)) {
				const auto way = plan.From(memory, player, place, margin, world.bodyPattern);
				Check(!way.path.empty(), "the map always has a way here");
				Check(LevelRoute::Ahead(way.path, grid, destination), "and its start is on loaded floor");
				const auto* cell = grid.At(static_cast<int>(destination.x), static_cast<int>(destination.y));
				Check(cell && cell->steps >= 0, "the walk heads for a cell the loaded floor reaches");
				led = true;
			}
			const auto route = Beacon::FindRoute(world, destination, settings);
			Check(!route.path.empty(), "a route to where the walk heads is found");
			player = route.path[std::min<size_t>(7, route.path.size() - 1)]; // some way along it before the next look
			const bool pocket = player.x > Left + 62 && player.x < Left + 138 && player.y > Top + 62 && player.y < Top + 150;
			if (pocket && !inside) ++entered;
			inside = pocket;
			arrived = Distance(player, place) <= 3;
		}
		Check(arrived, "the walk comes to the place beyond the ridge");
		Check(led, "fixture: the walk was led by the remembered map");
		// With two rooms each way the closed end is seen from outside the pocket.
		Check(entered == (reach == 1 ? 1 : 0), "it goes into the dead end only while the ridge has never been seen, once, and not again");
		Check(ticks <= 60, "and it walks no farther than in, out and round");
		Check(memory.At(Left + 100, Top + 61) == Ground::Blocked, "the ridge is remembered after the game has freed it");

		// The same walk begun again knows the ridge from the start.
		player = {Left + 100.0, Top + 180.0};
		const auto way = plan.From(memory, player, place, margin, 1);
		Check(!way.path.empty() && !way.guess, "a second walk has a way all seen from the first step");
		bool intoPocket = false;
		for (const auto point : way.path) intoPocket = intoPocket || (point.x > Left + 62 && point.x < Left + 138 && point.y > Top + 62 && point.y < Top + 150);
		Check(!intoPocket, "and it does not enter the dead end at all");
	}
	{
		// Ahead: as far along the way as the loaded floor goes.
		Level level;
		LevelRoute::Memory memory;
		Check(memory.Begin(1, 1, level.Rooms()), "fixture: the map begins");
		const Position player{Left + 20.0, Top + 20.0};
		const World world = level.Loaded(player, 0);
		(void)memory.Learn(world);
		const auto grid = Navigation::Build(world, Navigation::MaxRadius, Radar::Layer::Terrain, false);
		LevelRoute::Plan plan;
		const auto way = plan.From(memory, player, {Left + 20.0, Top + 150.0}, 1, 1);
		Position ahead;
		Check(way.guess && LevelRoute::Ahead(way.path, grid, ahead), "a way into unseen ground starts on loaded floor");
		Check(ahead.y == Top + RoomCells - 2, "the walk heads for the last cell the body stands on before the room not loaded");
		Check(!LevelRoute::Ahead({{Left + 20.0, Top + 60.0}}, grid, ahead), "a way that starts off the loaded floor leads nowhere yet");
		Check(!LevelRoute::Ahead({}, grid, ahead), "no way leads nowhere");
	}
	{
		// The Arcane Sanctuary, 2026-10-02, as a fixture: two walkways with a
		// void between them that only a teleport pad crosses, all of it loaded.
		using Answer = LevelRoute::Reach::Answer;
		Level level;
		level.Fill(Left, Top, Left + Width - 1, Top + Height - 1, Wall);
		level.Fill(Left + 10, Top + 10, Left + 70, Top + 20, 0);  // the walkway stood on
		level.Fill(Left + 80, Top + 10, Left + 150, Top + 20, 0); // the one past the void
		LevelRoute::Memory memory;
		Check(memory.Begin(1, 1, level.Rooms()), "fixture: the map begins");
		const Position player{Left + 20.0, Top + 15.0}, near{Left + 68.0, Top + 15.0}, far{Left + 82.0, Top + 15.0};
		(void)memory.Learn(level.Loaded(player, 2));
		Check(memory.At(Left + 82, Top + 15) == Ground::Open && memory.At(Left + 120, Top + 15) == Ground::Unseen,
			"fixture: the second walkway is loaded where it begins, and not at its far end");
		LevelRoute::Reach reach(memory, player, 1);
		Check(reach.To(near, 1) == Answer::Reached, "a pad on the walkway stood on is reached");
		Check(reach.To(far, 1) == Answer::CutOff, "a pad across the void is cut off");
		Check(reach.To({Left + 75.0, Top + 15.0}, 1) == Answer::CutOff, "and so is a place in the void itself");
		Check(reach.To({Left - 40.0, Top + 15.0}, 1) == Answer::Unknown, "a place outside the level's rooms cannot be told");
		Check(LevelRoute::Plan().From(memory, player, far, 1, 1).path.empty(), "the walk's own plan has no way where the reach says cut off");
		Check(!LevelRoute::Plan().From(memory, player, near, 1, 1).path.empty(), "and has one where it says reached");
		Check(reach.Holds(memory, near, 1), "the ground reached holds while the player stays on it");
		Check(!reach.Holds(memory, far, 1), "and not once a teleport pad has put the player on other ground");
		Check(!reach.Holds(memory, player, 2), "nor for another body");
		const LevelRoute::Reach across(memory, far, 1);
		Check(across.To(far, 1) == Answer::Reached && across.To(near, 1) == Answer::CutOff, "from the far walkway it is the first that is cut off");
		// A thing whose own cells are shut is reached from the cells beside it.
		level.Fill(Left + 40, Top + 14, Left + 42, Top + 16, 0x0400);
		(void)memory.Learn(level.Loaded(player, 2));
		Check(!reach.Holds(memory, player, 1), "a map that changed is searched again");
		reach = LevelRoute::Reach(memory, player, 1);
		Check(reach.To({Left + 41.0, Top + 15.0}, 2) == Answer::Reached, "a chest is reached from beside it");
		Check(reach.To({Left + 41.0, Top + 15.0}, 0) == Answer::CutOff, "fixture: its own cell is not stood on");
		// A room: any of its ground. The rooms are 40 cells a side.
		const auto rooms = level.Rooms();
		Check(reach.Within(NativeRooms::Subtiles(rooms[0])) == Answer::Reached, "the room stood in is reached");
		Check(reach.Within(NativeRooms::Subtiles(rooms[2])) == Answer::CutOff, "a room holding only the far walkway is cut off");
		Check(reach.Within(NativeRooms::Subtiles(rooms[Across])) == Answer::CutOff, "and so is a room of nothing but wall");
		// Ground not seen yet may have a way: nothing past it is called cut off.
		Level open;
		open.Fill(Left + 60, Top, Left + 62, Top + Height - 1, Wall); // a wall from top to bottom
		LevelRoute::Memory partly;
		Check(partly.Begin(1, 1, open.Rooms()), "fixture: the map begins");
		LevelRoute::Memory whole = partly;
		for (int row = 0; row < Down; ++row)
			for (int column = 0; column < Across; ++column)
				(void)whole.Learn(open.Loaded({Left + column * RoomCells + 5.0, Top + row * RoomCells + 5.0}, 0));
		Check(LevelRoute::Reach(whole, player, 1).To({Left + 70.0, Top + 15.0}, 1) == Answer::CutOff, "past a wall seen from end to end a place is cut off");
		(void)partly.Learn(open.Loaded(player, 1));
		Check(LevelRoute::Reach(partly, player, 1).To({Left + 70.0, Top + 15.0}, 1) == Answer::Reached,
			"with the wall seen in the rooms loaded only, the same place may be got to round its end");
		// Standing where the body does not fit, the ground beside is reached all the same.
		const Position against{Left + 10.0, Top + 10.0};
		const LevelRoute::Reach corner(memory, against, 1);
		Check(corner.To(near, 1) == Answer::Reached && corner.Holds(memory, against, 1), "a cell the body does not fit on can still be left");
		Check(LevelRoute::Reach(memory, {Left + 75.0, Top + 30.0}, 1).To(near, 1) == Answer::Unknown, "from inside a wall nothing can be told");
		Check(LevelRoute::Reach().To(near, 1) == Answer::Unknown && !LevelRoute::Reach().Holds(memory, player, 1), "nor with no map at all");
	}
	{
		// A door: no way is planned through one shut, and what lies past it is not cut off.
		using Answer = LevelRoute::Reach::Answer;
		constexpr uint16_t Door = 0x0800, Object = 0x0400;
		Level level;
		level.Fill(Left, Top + 60, Left + Width - 1, Top + 62, Wall); // a wall across the level
		level.Fill(Left + 50, Top + 60, Left + 52, Top + 62, Door);   // and a doorway in it, its door shut
		level.At(Left + 51, Top + 61) = Door | Object;
		LevelRoute::Memory memory;
		Check(memory.Begin(1, 1, level.Rooms()), "fixture: the map begins");
		for (int row = 0; row < Down; ++row)
			for (int column = 0; column < Across; ++column)
				(void)memory.Learn(level.Loaded({Left + column * RoomCells + 5.0, Top + row * RoomCells + 5.0}, 0));
		Check(memory.At(Left + 50, Top + 60) == Ground::Door && memory.At(Left + 51, Top + 61) == Ground::Door, "a shut door is remembered as a door");
		level.At(Left + 100, Top + 30) = Object;
		level.At(Left + 101, Top + 30) = Door | Wall;
		(void)memory.Learn(level.Loaded({Left + 100.0, Top + 30.0}, 0));
		Check(memory.At(Left + 100, Top + 30) == Ground::Blocked && memory.At(Left + 101, Top + 30) == Ground::Blocked,
			"an object alone, and a door on a wall, are ground seen shut");
		const Position player{Left + 51.0, Top + 100.0}, place{Left + 51.0, Top + 20.0};
		Check(LevelRoute::Plan().From(memory, player, place, 1, 1).path.empty(), "no way is planned through a shut door");
		Check(LevelRoute::Reach(memory, player, 1).To(place, 1) == Answer::Reached, "what lies past a shut door is not cut off");
		level.Fill(Left + 50, Top + 60, Left + 52, Top + 62, 0); // the door opened
		(void)memory.Learn(level.Loaded({Left + 51.0, Top + 61.0}, 0));
		Check(!LevelRoute::Plan().From(memory, player, place, 1, 1).path.empty(), "an opened door is a way");
	}
	{
		// The Arcane Sanctuary's pads, as measured 2026-10-02: each gap is crossed
		// by a pad on shut ground at the end of one walkway (25145,5387) and a pad
		// on the floor of the next (25156,5387), and each leads to the other.
		using Answer = LevelRoute::Reach::Answer;
		using LevelRoute::Link;
		using LevelRoute::Pad;
		Level level;
		level.Fill(Left, Top, Left + Width - 1, Top + Height - 1, Wall);
		level.Fill(Left + 10, Top + 10, Left + 40, Top + 20, 0);  // walkway A, stood on
		level.Fill(Left + 60, Top + 10, Left + 90, Top + 20, 0);  // B, past a void
		level.Fill(Left + 110, Top + 10, Left + 140, Top + 20, 0); // C, past another
		LevelRoute::Memory memory;
		Check(memory.Begin(1, 1, level.Rooms()), "fixture: the map begins");
		for (int row = 0; row < Down; ++row)
			for (int column = 0; column < Across; ++column)
				(void)memory.Learn(level.Loaded({Left + column * RoomCells + 5.0, Top + row * RoomCells + 5.0}, 0));
		const Pad edgeA{Left + 41, Top + 15, 192}, floorB{Left + 62, Top + 15, 192};
		const Pad edgeB{Left + 91, Top + 15, 304}, floorC{Left + 112, Top + 15, 304};
		const std::vector<Link> both{{edgeA, floorB}, {floorB, edgeA}, {edgeB, floorC}, {floorC, edgeB}};
		const std::vector<Link> first{{edgeA, floorB}, {floorB, edgeA}};
		const Position player{Left + 15.0, Top + 15.0}, onA{Left + 25.0, Top + 15.0}, onB{Left + 80.0, Top + 15.0}, onC{Left + 130.0, Top + 15.0};
		const LevelRoute::Reach walking(memory, player, 1);
		Check(walking.To(onB, 1) == Answer::CutOff && walking.To(onC, 1) == Answer::CutOff, "walking alone, both walkways past the voids are cut off");
		const LevelRoute::Reach pads(memory, player, 1, both, 3, 7);
		Pad pad;
		Check(pads.To(onB, 1) == Answer::Reached && pads.Via(onB, 1, pad) && pad == edgeA,
			"through the pads B is reached, by the pad at the end of A");
		Check(pads.To(onC, 1) == Answer::Reached && pads.Via(onC, 1, pad) && pad == edgeA,
			"and C, two pads on: the pad to use first is still the one on A");
		int toB = 0, toC = 0;
		Check(pads.Via(onB, 1, pad, &toB) && pads.Via(onC, 1, pad, &toC) && toB > 0 && toC > toB,
			"the way on to C is longer than the way to B");
		Check(pads.To(onA, 1) == Answer::Reached && !pads.Via(onA, 1, pad), "a place walked to takes no pad");
		const auto rooms = level.Rooms();
		Check(pads.Within(NativeRooms::Subtiles(rooms[3])) == Answer::Reached && pads.ViaWithin(NativeRooms::Subtiles(rooms[3]), pad) && pad == edgeA,
			"a room holding only C is reached the same way");
		const LevelRoute::Reach fromB(memory, onB, 1, both, 3, 7);
		Check(fromB.Via(onC, 1, pad) && pad == edgeB, "from B, C is by B's own pad");
		Check(fromB.To(onA, 1) == Answer::Reached && fromB.Via(onA, 1, pad) && pad == floorB,
			"and the way back to A is the pad on B's floor, which lands beside the pad on shut ground");
		const LevelRoute::Reach half(memory, player, 1, first, 3, 7);
		Check(half.To(onB, 1) == Answer::Reached && half.To(onC, 1) == Answer::CutOff, "a pad with no way known takes no one anywhere");
		Check(LevelRoute::Reach(memory, player, 1, both, 1, 7).To(onB, 1) == Answer::CutOff,
			"fixture: from within one cell no body stands by the pad on shut ground");
		// With no pads the search marks every cell as the search on foot does.
		const LevelRoute::Reach none(memory, player, 1, {}, 3, 7);
		bool same = true;
		for (int y = Top - 2; y < Top + Height + 2 && same; ++y)
			for (int x = Left - 2; x < Left + Width + 2 && same; ++x)
				same = none.To({static_cast<double>(x), static_cast<double>(y)}, 0) == walking.To({static_cast<double>(x), static_cast<double>(y)}, 0);
		for (const auto& room : rooms) same = same && none.Within(NativeRooms::Subtiles(room)) == walking.Within(NativeRooms::Subtiles(room));
		Check(same, "with no pads every cell and room answers as the search on foot");
		Check(pads.Holds(memory, player, 1, 7) && !pads.Holds(memory, {Left + 16.0, Top + 15.0}, 1, 7) && !pads.Holds(memory, player, 1, 8),
			"the search through pads holds from its own cell, with the same pads");
		Check(!pads.Holds(memory, player, 1) && !walking.Holds(memory, player, 1, 7), "and the two searches never stand in for each other");
	}
	Passed();
	return 0;
}
