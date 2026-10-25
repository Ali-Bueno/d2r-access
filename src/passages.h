#pragma once

#include <array>
#include <functional>

// Corridors, bends, junctions and dead ends, read from the shape of the walkable
// grid around the player: nothing in the game names them. D2 walls follow the
// world axes, so a corridor runs along one of them. Pure logic, no native calls.
// docs/places.md#corridors
namespace D2RAccess::Passages {

enum class Ground : unsigned char { Unknown, Open, Blocked };
using Probe = std::function<Ground(int x, int y)>;

struct Settings {
	int minWidth = 0;  // narrower floor is a slit along a wall, not a corridor (subtiles)
	int maxWidth = 0;  // widest floor, wall to wall, that is still a corridor
	int minLength = 0; // shortest narrow stretch that is a corridor and not a doorway
	int slack = 0;     // how much wider than a corridor a bend or junction may be
	int reach = 0;     // how far a run or a corridor is followed before giving up
	int passable = 0;  // the body's width: an opening narrower than this past a wall is no way on
};

enum class Shape : unsigned char { None, Corridor, Bend, Junction };
enum class End : unsigned char { Opens, Closed, Unknown };

struct Way {
	int x = 0, y = 0; // the last corridor cell before the end
	End end = End::Unknown;
};

struct Reading {
	Shape shape = Shape::None;
	bool alongX = false;     // a corridor's axis
	std::array<Way, 2> ends; // a corridor's two ends: -axis, +axis
	int arms = 0;            // corridors leaving a bend or junction
	bool DeadEnd() const { return shape == Shape::Corridor && (ends[0].end == End::Closed || ends[1].end == End::Closed); }
};

Reading Classify(const Probe& probe, int x, int y, const Settings& settings);
}
