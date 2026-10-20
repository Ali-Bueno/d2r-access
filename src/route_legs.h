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

// Whether a leg may cross a cell: ground the way itself may cross, with room
// for the whole body.
using Passable = std::function<bool(int x, int y)>;
// Where a way ends: the cells, bounds included, that count as being there
// (LevelRoute::Area); with `counts`, only those it says do (LevelRoute::Arrival).
struct End {
	int minX = 0, minY = 0, maxX = 0, maxY = 0;
	std::function<bool(int x, int y)> counts;
};

// The way from `from` along `path` (the cells to walk, the last at the place)
// as legs the player walks with the stick or the arrows: each one of their
// eight ways, over `passable` ground near the path, with as few turns as the
// ground allows, split at the shut doors (`doors`: indices into `path`). It
// ends anywhere in `end`, else at the path's last cell.
std::vector<Leg> Describe(Position from, const std::vector<Position>& path, const std::vector<size_t>& doors,
	const Passable& passable, SpokenDirection::Style style, const End* end = nullptr);
double Length(const std::vector<Leg>& legs);
// One leg: "north, 20", or "door".
std::string Say(const Leg& leg, const Strings::Table& strings);
// What a route asked for says besides its target: its whole length, its legs (both switches).
struct Wording { bool total = true, legs = true; };
// "Route to Akara, 85 units: north, 20; then door; then east, 30", or as much of it as `wording`
// keeps: "Route to Akara: north, 20; ...", "Route to Akara, 85 units", "Route to Akara".
// `unseen`: part of the way crosses ground not seen yet.
std::string Speak(const std::string& name, const std::vector<Leg>& legs, bool unseen, Wording wording, const Strings::Table& strings);
}
