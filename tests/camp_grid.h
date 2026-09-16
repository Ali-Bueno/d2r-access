#pragma once
#include "radar.h"
#include <string_view>

// Exact Ctrl+T capture, 2026-09-15 21:06:53 at (4472,4622). Only terrain
// classification was logged; original collision bits are unavailable.
inline D2RAccess::Radar::World RecordedCampGrid() {
	constexpr std::array<std::string_view,25> rows{
		"...........##############", "............#############", "............#############",
		"...............##########", "...............##########", "............#############",
		"............###.......###", ".............##........##", "........................#",
		"........................#", "########.................", "########.................",
		"............@............", ".........................", ".........................",
		"####.#...................", "####.#...................", "####.....................",
		"####.....................", "......#..................", ".........................",
		".........................", ".........................", ".........................",
		"........................."};
	D2RAccess::Radar::World world;
	world.player = {4472,4622}; world.session = world.level = 1;
	world.bodyPattern = 1; world.walkingMask = 0x1C09; // recorded native pattern and mask
	world.rooms = {{4460,4610,25,25,std::vector<uint16_t>(625)}};
	for (size_t y=0; y<rows.size(); ++y) for (size_t x=0; x<rows[y].size(); ++x)
		world.rooms[0].cells[y*25+x] = rows[y][x] == '#' ? 1 : 0;
	return world;
}
