#pragma once

#include "radar.h"
#include "speech_strings.h"
#include "spoken_direction.h"

#include <functional>
#include <string>
#include <vector>

// A walking way said the way a sighted player reads it off the map: a few
// straight legs, each a direction and a length in units, and the shut doors
// on it. Pure logic, unit-tested without the game. docs/route-description.md
namespace D2RAccess::RouteLegs {
using Radar::Position;

struct Leg {
	Position delta;     // the way the leg heads, world subtiles
	double length = 0;  // units walked along it
	bool door = false;  // a shut door to open, said as a leg of its own
};

// Whether a straight line may cut across a cell: ground the way itself may cross.
using Passable = std::function<bool(int x, int y)>;

// The way from `from` along `path` (the cells to walk, the last at the place),
// straightened over `passable` ground, split at the shut doors (`doors`:
// indices into `path`), and merged into legs as `style` names their directions.
std::vector<Leg> Describe(Position from, const std::vector<Position>& path, const std::vector<size_t>& doors,
	const Passable& passable, SpokenDirection::Style style);
double Length(const std::vector<Leg>& legs);
// "Route to Akara, 85 units: north, 20; then door; then east, 30". `unseen`:
// part of the way crosses ground not seen yet.
std::string Speak(const std::string& name, const std::vector<Leg>& legs, bool unseen, const Strings::Table& strings);
}
