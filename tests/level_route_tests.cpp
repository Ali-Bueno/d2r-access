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
	Passed();
	return 0;
}
