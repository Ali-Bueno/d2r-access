#include "explored_rooms.h"
#include "level_exit_layout.h"
#include "spoken_direction.h"
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

// Thirds along each world axis: the level's rectangle is a diamond on screen,
// its sides face the screen diagonals and its corners the screen cardinals.
void Places() {
	const auto layout = Level({});
	constexpr int Last = Origin + Rooms * RoomTiles - 1;
	Check(Place(layout, At(Origin + 6, Origin + 6)) == 4, "the middle of the level is the centre");
	Check(Place(layout, At(Origin, Origin)) == 0, "the world origin corner is the north corner on screen");
	Check(Place(layout, At(Last, Last)) == 8, "the far world corner is the south corner");
	Check(Place(layout, At(Last, Origin)) == 6, "growing X alone is the east corner");
	Check(Place(layout, At(Origin, Last)) == 2, "growing Y alone is the west corner");
	Check(Place(layout, At(Origin + 6, Origin)) == 3, "the middle of the near Y side is the northeast edge");
	Check(Place(layout, At(Origin, Origin + 6)) == 1, "the middle of the near X side is the northwest edge");
	Check(std::string(English.terrainPlaces[3]) == "northeast edge" && std::string(English.terrainPlaces[1]) == "northwest edge",
		"the place words follow that order");
}

// One utterance: where the player stands, then each side of the level with how
// far it reaches and where the unwalked part starts, then the places found.
void Spoken() {
	auto layout = Level({{104, 104}, {100, 100}});
	layout.neighbours.push_back({{104, 112, RoomTiles, RoomTiles}, "Rogue Encampment"});
	const auto summary = TerrainOverview::Describe(layout, At(106, 106));
	Check(summary.valid && summary.place == 4, "the fixture puts the player in the centre");
	const std::string text = TerrainOverview::Speak(summary, "Blood Moor", Scan, English);
	Check(text == "Blood Moor, centre, 22 percent explored. northeast: edge at 35 units, unexplored from 15 units."
		" southeast: edge at 30 units, unexplored from 10 units."
		" southwest: Rogue Encampment at 30 units, unexplored from 10 units."
		" northwest: edge at 35 units, unexplored from 15 units",
		"the overview names the level, the place, the share seen and each side");
	Check(summary.percent == 22, "two of nine rooms seen is 22 percent, rounded down");
	Check(TerrainOverview::Describe(Level({}), At(106, 106)).percent == 0, "nothing seen is 0 percent");
	Check(TerrainOverview::Speak(summary, "Blood Moor", Scan, Spanish)
		== "Blood Moor, centro, explorado 22 por ciento. noreste: borde a 35 unidades, sin explorar desde 15 unidades."
		" sureste: borde a 30 unidades, sin explorar desde 10 unidades."
		" suroeste: Rogue Encampment a 30 unidades, sin explorar desde 10 unidades."
		" noroeste: borde a 35 unidades, sin explorar desde 15 unidades",
		"the wording comes from the speech table, not the logic");
	SpokenDirection::SetStyle(SpokenDirection::Style::Clock);
	Check(TerrainOverview::Speak(summary, "Blood Moor", Scan, Spanish).find(". a las 2: borde a 35 unidades, sin explorar desde 15 unidades. a las 4: borde")
		!= std::string::npos, "the clock style names the sides by their hours");
	SpokenDirection::SetStyle(SpokenDirection::Style::Compass);
	const std::string found = TerrainOverview::Speak(summary, "Blood Moor", Scan, English,
		{{"Waypoint", {0, -20}}, {"Forgotten Tower", {10, 10}}, {"", {1, 1}}});
	Check(found.ends_with(" northwest: edge at 35 units, unexplored from 15 units. Forgotten Tower, 14 units, south. Waypoint, 20 units, northeast"),
		"the places found follow the sides, nearest first, with their distance and direction; a nameless one is left out");
}

void Directions() {
	const auto layout = Level({{104, 104}, {100, 104}});
	const auto summary = TerrainOverview::Describe(layout, At(106, 106));
	const auto& ahead = summary.legs[static_cast<size_t>(TerrainOverview::Side::Northwest)];
	const auto& behind = summary.legs[static_cast<size_t>(TerrainOverview::Side::Southeast)];
	Check(ahead.exploredToEdge && !ahead.hasUnexplored, "walked rooms all the way out read as explored");
	Check(ahead.edge > behind.edge, "the level reaches further towards the world origin from here");
	Check(behind.hasUnexplored && !behind.exploredToEdge, "a room never streamed is unexplored terrain");
	Check(behind.border.empty(), "no neighbour level is named where none was generated");
	// Only the first unwalked room counts: what lies behind it is unknown too.
	const auto fresh = TerrainOverview::Describe(Level({}), At(106, 106));
	const auto& first = fresh.legs[static_cast<size_t>(TerrainOverview::Side::Northwest)];
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
	layout.neighbours.push_back({{104, 112, RoomTiles, RoomTiles}, "Rogue Encampment"});
	const auto summary = TerrainOverview::Describe(layout, At(106, 106));
	Check(summary.legs[static_cast<size_t>(TerrainOverview::Side::Southwest)].border == "Rogue Encampment",
		"the level beyond the edge is named where the game has generated it");
	Check(summary.legs[static_cast<size_t>(TerrainOverview::Side::Northeast)].border.empty(),
		"the opposite edge names nothing");
	Check(TerrainOverview::Speak(summary, "Blood Moor", Scan, English).find("Rogue Encampment at 30 units")
		!= std::string::npos, "a named border replaces the plain edge");
	const std::string listed = TerrainOverview::Speak(summary, "Blood Moor", Scan, English,
		{{"Rogue Encampment", {0, 30}}, {"Den of Evil", {10, 0}}});
	Check(listed.find("Rogue Encampment") == listed.rfind("Rogue Encampment"), "a border found as a crossing is said once");
	Check(listed.find("Den of Evil") != std::string::npos, "other landmarks are still listed");
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

// The share is drawn tiles over drawable tiles, rooms never streamed estimated by density.
void Tiles() {
	constexpr size_t RoomArea = RoomTiles * RoomTiles;
	// Entry state: three of nine rooms charted, a sliver of each drawn, the rest unmeasured.
	auto entry = Level({{100, 100}, {104, 100}, {108, 100}});
	for (size_t i = 0; i < entry.rooms.size(); ++i) {
		auto& room = entry.rooms[i];
		room.measured = i < 3;
		room.drawable = room.measured ? RoomArea : 0;
		room.drawn = room.measured ? 2 : 0;
	}
	const auto first = TerrainOverview::Describe(entry, At(Origin, Origin));
	Check(first.byTiles && first.drawableTiles == 9 * RoomArea && first.estimatedTiles == 6 * RoomArea && first.drawnTiles == 6,
		"unmeasured rooms are estimated at the measured rooms' density");
	Check(first.percent == 4, "a few drawn tiles of charted rooms read well under the room share (33 percent)");

	auto half = Level({});
	for (auto& room : half.rooms) { room.explored = true; room.measured = true; room.drawable = RoomArea; room.drawn = RoomArea / 2; }
	Check(TerrainOverview::Describe(half, At(Origin, Origin)).percent == 50, "rooms all visited but half drawn read about half");

	auto sparse = half;
	for (auto& room : sparse.rooms) room.drawable = RoomArea / 2;
	sparse.rooms.back().measured = false;
	const auto estimated = TerrainOverview::Describe(sparse, At(Origin, Origin));
	Check(estimated.estimatedTiles == RoomArea / 2 && estimated.drawableTiles == 9 * RoomArea / 2,
		"the estimate follows the mean drawable density, not the room area");
	Check(TerrainOverview::Describe(Level({{100, 100}}), At(Origin, Origin)).percent == 11 && !TerrainOverview::Describe(Level({}), At(Origin, Origin)).byTiles,
		"with no measured room the room count is kept");

	// Charted in an earlier game (a tester, 2026-10-24: a full automap read 43 percent): what the
	// automap kept of a room not streamed again counts as drawn, and its estimate is never less.
	auto earlier = sparse;
	for (auto& room : earlier.rooms) room.drawn = RoomArea / 2;
	Check(TerrainOverview::Describe(earlier, At(Origin, Origin)).percent == 100, "a whole map is 100 percent with a room not streamed");
	earlier.rooms.back().drawn = RoomArea;
	const auto denser = TerrainOverview::Describe(earlier, At(Origin, Origin));
	Check(denser.percent == 100 && denser.estimatedTiles == RoomArea, "an estimate never falls below what is drawn");
}

int main() {
	Tiles();
	Extent();
	Places();
	Spoken();
	Directions();
	Borders();
	Explored();
	Passed();
	return 0;
}
