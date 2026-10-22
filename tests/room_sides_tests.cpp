#include "room_sides.h"
#include "speech_strings.h"
#include "spoken_direction.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace D2RAccess;
using Radar::Position;

namespace {
const Strings::Table& English = Strings::For(Strings::Language::enUS);

// The map's sides (a tester, 2026-10-14): the room's own walls, which D2 draws at the
// screen diagonals, said as those diagonals or as their hours.
void MapSides() {
	// A room from 80,80 to 119,119 with a passage in each wall.
	const std::vector<Rooms::Opening> openings{
		{{110, 80}, 3, {110, 79}},  // top wall, world -Y
		{{119, 85}, 3, {120, 85}},  // right wall, +X
		{{90, 119}, 3, {90, 120}},  // bottom wall, +Y
		{{80, 112}, 3, {79, 112}}}; // left wall, -X
	const auto side = RoomSides::MapSide({3, -10});
	Check(side.x == 0 && side.y == -1, "an offset names the one world axis it points to most");
	Check(RoomSides::Wall(openings[0]).y == -1 && RoomSides::Wall(openings[1]).x == 1
		&& RoomSides::Wall(openings[2]).y == 1 && RoomSides::Wall(openings[3]).x == -1, "a passage names its own wall");
	Check(std::string(SpokenDirection::Of(RoomSides::Wall(openings[0]), English)) == "northeast"
		&& std::string(SpokenDirection::Of(RoomSides::Wall(openings[3]), English)) == "northwest",
		"the walls are said as the screen diagonals");
	SpokenDirection::SetStyle(SpokenDirection::Style::Clock);
	Check(std::string(SpokenDirection::Of(RoomSides::Wall(openings[0]), English)) == "2 o'clock"
		&& std::string(SpokenDirection::Of(RoomSides::Wall(openings[2]), English)) == "8 o'clock",
		"and as the hours they are drawn at");
	SpokenDirection::SetStyle(SpokenDirection::Style::Compass);
}
}

int main() {
	MapSides();
	Passed();
}
