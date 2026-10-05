#include "explored_rooms.h"
#include "level_exit_layout.h"
#include "terrain_overview.h"
#include "test_support.h"

#include <initializer_list>
#include <string>
#include <utility>

using namespace D2RAccess;
using Radar::Position;
using TerrainOverview::Layout;
using TerrainOverview::Summary;

namespace {
const Strings::Table& English = Strings::For(Strings::Language::enUS);
const Strings::Table& Spanish = Strings::For(Strings::Language::esES);
const EntityRadar::Settings Scan{};
constexpr int Tile = LevelExitLayout::SubtilesPerTile;
constexpr int RoomTiles = 4, Origin = 100, Rooms = 3;

// Nine rooms of four tiles each, so the level spans twelve tiles per side;
// only the rooms listed as explored have ever been streamed.
Layout Level(std::initializer_list<std::pair<int, int>> explored) {
	Layout layout;
	for (int row = 0; row < Rooms; ++row)
		for (int column = 0; column < Rooms; ++column) {
			const int x = Origin + column * RoomTiles, y = Origin + row * RoomTiles;
			bool walked = false;
			for (const auto& room : explored) walked = walked || (room.first == x && room.second == y);
			layout.rooms.push_back({{x, y, RoomTiles, RoomTiles}, walked});
		}
	return layout;
}
// The subtile position of a tile's middle.
Position At(int tileX, int tileY) {
	return {static_cast<double>(tileX * Tile + Tile / 2), static_cast<double>(tileY * Tile + Tile / 2)};
}
size_t Place(const Layout& layout, Position player) { return TerrainOverview::Describe(layout, player).place; }

void Extent() {
	const auto layout = Level({});
	const auto box = TerrainOverview::Bounds(layout.rooms);
	Check(box.x == Origin && box.y == Origin && box.width == Rooms * RoomTiles && box.height == Rooms * RoomTiles,
		"the level's extent is the union of its rooms");
	Check(TerrainOverview::Bounds({}).width == 0, "a level with no rooms has no extent");
	Check(!TerrainOverview::Describe(layout, At(Origin - 1, Origin - 1)).valid,
		"a player outside every room has no overview");
	Check(TerrainOverview::Describe(layout, At(Origin, Origin)).valid, "a player inside a room has one");
}

// Screen thirds, over the same axes the radar and the exit review use: the
// level's tile rectangle is a diamond on screen, so its corners are its poles.
void Places() {
	const auto layout = Level({});
	constexpr int Last = Origin + Rooms * RoomTiles - 1;
	Check(Place(layout, At(Origin + 6, Origin + 6)) == 4, "the middle of the level is the centre");
	Check(Place(layout, At(Origin, Origin)) == 7, "the world origin corner is the ahead edge on screen");
	Check(Place(layout, At(Last, Last)) == 1, "the far world corner is the edge behind");
	Check(Place(layout, At(Last, Origin)) == 5, "growing X alone is the right edge");
	Check(Place(layout, At(Origin, Last)) == 3, "growing Y alone is the left edge");
}

// One utterance: where the player stands, then each screen direction with how
// far the level reaches and where the unwalked part starts.
void Spoken() {
	auto layout = Level({{104, 104}, {100, 100}});
	layout.neighbours.push_back({{112, 112, RoomTiles, RoomTiles}, "Rogue Encampment"});
	const auto summary = TerrainOverview::Describe(layout, At(106, 106));
	Check(summary.valid && summary.place == 4, "the fixture puts the player in the centre");
	const std::string text = TerrainOverview::Speak(summary, "Blood Moor", Scan, English);
	Check(text == "Blood Moor, centre, 22 percent mapped. ahead: edge at 49 steps, explored."
		" to the left: edge at 42 steps, unexplored from 14 steps."
		" to the right: edge at 42 steps, unexplored from 14 steps."
		" behind: Rogue Encampment at 42 steps, unexplored from 14 steps",
		"the overview names the level, the place, the share seen and each direction");
	Check(summary.percent == 22, "two of nine rooms seen is 22 percent, rounded down");
	Check(TerrainOverview::Describe(Level({}), At(106, 106)).percent == 0, "nothing seen is 0 percent");
	Check(TerrainOverview::Speak(summary, "Blood Moor", Scan, Spanish)
		== "Blood Moor, centro, 22 por ciento cartografiado. adelante: borde a 49 pasos, explorado."
		" a la izquierda: borde a 42 pasos, sin explorar desde 14 pasos."
		" a la derecha: borde a 42 pasos, sin explorar desde 14 pasos."
		" atrás: Rogue Encampment a 42 pasos, sin explorar desde 14 pasos",
		"the wording comes from the speech table, not the logic");
}

void Directions() {
	const auto layout = Level({{104, 104}, {100, 100}});
	const auto summary = TerrainOverview::Describe(layout, At(106, 106));
	const auto& ahead = summary.legs[static_cast<size_t>(Radar::Direction::Up)];
	const auto& behind = summary.legs[static_cast<size_t>(Radar::Direction::Down)];
	Check(ahead.exploredToEdge && !ahead.hasUnexplored, "walked rooms all the way out read as explored");
	Check(ahead.edge > behind.edge, "the level reaches further towards the world origin from here");
	Check(behind.hasUnexplored && !behind.exploredToEdge, "a room never streamed is unexplored terrain");
	Check(behind.border.empty(), "no neighbour level is named where none was generated");
	// Only the first unwalked room counts: what lies behind it is unknown too.
	const auto fresh = TerrainOverview::Describe(Level({}), At(106, 106));
	const auto& first = fresh.legs[static_cast<size_t>(Radar::Direction::Up)];
	Check(first.hasUnexplored && first.unexplored < 10, "an unexplored stretch can start at the first step");
	Check(TerrainOverview::Speak(fresh, "Blood Moor", Scan, English).find("unexplored from") == std::string::npos,
		"a stretch unexplored from the player's feet is spoken without a distance");
	Check(TerrainOverview::Speak(fresh, "Blood Moor", Scan, English).find("unexplored") != std::string::npos,
		"but it is still spoken as unexplored");
}

// A generated level across the border is named, exactly as the exit review
// names it; an ungenerated one is simply the edge.
void Borders() {
	auto layout = Level({});
	layout.neighbours.push_back({{112, 112, RoomTiles, RoomTiles}, "Rogue Encampment"});
	const auto summary = TerrainOverview::Describe(layout, At(106, 106));
	Check(summary.legs[static_cast<size_t>(Radar::Direction::Down)].border == "Rogue Encampment",
		"the level beyond the edge is named where the game has generated it");
	Check(summary.legs[static_cast<size_t>(Radar::Direction::Up)].border.empty(),
		"the opposite edge names nothing");
	Check(TerrainOverview::Speak(summary, "Blood Moor", Scan, English).find("Rogue Encampment at 42 steps")
		!= std::string::npos, "a named border replaces the plain edge");
}

void Explored() {
	ExploredRooms::Memory memory;
	Check(memory.Mark(1, 10, 20), "a room is new the first time it is streamed");
	Check(!memory.Mark(1, 10, 20), "and known from then on");
	Check(memory.Explored(1, 10, 20) && !memory.Explored(1, 10, 21), "rooms are keyed by their tile origin");
	Check(!memory.Explored(2, 10, 20), "and by their level, so two levels never share a map");
	memory.Mark(2, 30, 40);
	Check(memory.Count(1) == 1 && memory.Count(2) == 1 && memory.Count(3) == 0, "each level counts its own rooms");
	memory.Forget();
	Check(!memory.Explored(1, 10, 20) && memory.Count(1) == 0, "a new session starts from an unexplored map");
}

}

int main() {
	Extent();
	Places();
	Spoken();
	Directions();
	Borders();
	Explored();
	Passed();
	return 0;
}
