#pragma once

#include "entity_radar.h"
#include "native_level_exits.h"
#include "radar.h"
#include "speech_strings.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

// The automap read aloud: where in the level the player stands, how far the
// terrain reaches towards each side of the level, where the part they have not
// walked yet begins, and the places already found. Named neighbouring levels
// only, never a route. Engine-free, so it is unit-tested without the game.
// `docs/terrain-overview.md`
namespace D2RAccess::TerrainOverview {
using NativeLevelExits::TileRect;
using Radar::Position;

struct Room { TileRect rect; bool explored = false; };
// A room of another level that the game has already generated, so naming it
// gives nothing away, exactly as the exit review does.
struct Neighbour { TileRect rect; std::string name; };
struct Layout { std::vector<Room> rooms; std::vector<Neighbour> neighbours; };

// The level's rectangle lies along the world axes, which the game draws as a
// diamond: each of its sides faces a screen diagonal (a tester, 2026-10-14:
// the edges are northeast, southeast, southwest and northwest).
enum class Side { Northeast, Southeast, Southwest, Northwest };
inline constexpr size_t SideCount = 4;
// The world step towards each side: -y is up-right on screen, +x down-right (Radar::ScreenOffset).
inline constexpr std::array<Position, SideCount> SideSteps{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

// What one side looks like from where the player stands.
struct Leg {
	double edge = 0;           // subtiles to where the level stops
	double unexplored = 0;     // subtiles to the first room never streamed
	bool hasUnexplored = false;
	bool exploredToEdge = false; // walked terrain all the way to the edge
	std::string border;          // the level across the edge, when it is generated
};

// Where in the level the player stands: thirds along each world axis, so nine
// places, named by the sides and corners they are near.
inline constexpr int ThirdsPerAxis = 3;
static_assert(ThirdsPerAxis * ThirdsPerAxis == Strings::TerrainPlaceCount);

struct Summary {
	bool valid = false;
	size_t place = 0;  // index into Strings::Table::terrainPlaces
	int percent = 0;   // share of the level's rooms ever streamed, what the automap would show filled
	TileRect box;      // the level's extent, the union of its rooms
	std::array<Leg, SideCount> legs{}; // Side order
};

// A place the scanner has already logged in this level, as the automap shows
// it: an exit, a waypoint, the stash, a quest object.
struct Landmark { std::string name; Position offset; }; // offset from the player, in subtiles

// The union of the rooms' tile rectangles: the level's extent.
TileRect Bounds(const std::vector<Room>& rooms);
// Invalid when the player stands in no room of this level.
Summary Describe(const Layout& layout, Position player);
// One spoken line: the level, the place and share seen, each side, then the
// landmarks found, nearest first, with their distance and direction.
std::string Speak(const Summary& summary, const std::string& levelName,
	const EntityRadar::Settings& scan, const Strings::Table& strings, std::vector<Landmark> landmarks = {});
}
