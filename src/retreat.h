#pragma once

#include "navigation_grid.h"
#include "radar.h"

#include <array>
#include <cstddef>
#include <vector>

// The way back to the nearest way out, for a player who cannot use rooms and
// corners to break up a pack: the fight is taken to the exit, where leaving is
// one key away. The route is the cheapest, not the shortest: ground near a
// monster costs more, so the walk goes round a pack when there is a way round
// and through it only when there is none. Pure logic over the reachability
// grid the scanner already builds, so it is unit-tested without the game.
namespace D2RAccess::Retreat {
using Radar::Position;

// Navigation policy, not game constants. Distances are collision subtiles.
struct Settings {
	int stepCost = 10; // one straight step; a diagonal costs sqrt2 of it
	// What each monster adds to the cost of a cell, by how near it stands: within
	// reach of a blow, within a stride of one, and near enough to be drawn in.
	struct Tier { double within = 0; int cost = 0; };
	std::array<Tier, 3> danger{{{3, 80}, {6, 25}, {9, 8}}};
	double arrival = 3; // a way out is reached from this near: a warp's own cell may be wall
};

struct Plan {
	bool found = false;
	size_t goal = 0;         // index into the goals given
	bool threatened = false; // the route passes within a blow of a monster
	double length = 0;       // walked length of the route, in subtiles
	std::vector<Position> path; // from the cell after the player's to the cell beside the way out
};

// The cheapest route from the grid's player cell to any of `goals`. Build the
// grid on the terrain layer: a monster is a cost here, never a wall, since it
// will have moved by the time the player gets there.
Plan Find(const Navigation::Grid& grid, const std::vector<Position>& goals,
	const std::vector<Position>& threats, const Settings& settings = {});

// Where a pack can only come a few at a time: corridors and doorways. A cell
// is one when walls stand on both sides of it no more than `width` apart,
// along a world axis or a diagonal, and the floor runs on for `through` cells
// both ways across that, so the corner of a room and a niche in a wall are
// not taken for a passage. Walls the map has not shown yet do not count: an
// unread edge is not a wall.
struct Narrows {
	int width = 7;   // the widest gap still called narrow, in cells the body fits in
	int through = 4; // floor needed each way along the passage
};
// Every such cell the player can walk to, as goals for Find. Build the grid on
// the terrain layer, as for Find.
std::vector<Position> Chokepoints(const Navigation::Grid& grid, const Narrows& narrows = {});
}
