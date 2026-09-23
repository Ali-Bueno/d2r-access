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
// terrain reaches along each screen direction, and where the part they have
// not walked yet begins. Named neighbouring levels only, never a route.
// Engine-free, so it is unit-tested without the game. `docs/terrain-overview.md`
namespace D2RAccess::TerrainOverview {
using NativeLevelExits::TileRect;
using Radar::Position;

struct Room { TileRect rect; bool explored = false; };
// A room of another level that the game has already generated, so naming it
// gives nothing away, exactly as the exit review does.
struct Neighbour { TileRect rect; std::string name; };
struct Layout { std::vector<Room> rooms; std::vector<Neighbour> neighbours; };

// What one screen direction looks like from where the player stands.
struct Leg {
	double edge = 0;           // subtiles to where the level stops
	double unexplored = 0;     // subtiles to the first room never streamed
	bool hasUnexplored = false;
	bool exploredToEdge = false; // walked terrain all the way to the edge
	std::string border;          // the level across the edge, when it is generated
};

// Where in the level the player stands: screen thirds, so nine places.
inline constexpr int ThirdsPerAxis = 3;
static_assert(ThirdsPerAxis * ThirdsPerAxis == Strings::TerrainPlaceCount);

struct Summary {
	bool valid = false;
	size_t place = 0;  // index into Strings::Table::terrainPlaces
	int percent = 0;   // share of the level's rooms ever streamed, what the automap would show filled
	TileRect box;      // the level's extent, the union of its rooms
	std::array<Leg, Radar::DirectionCount> legs{}; // Radar::Direction order
};

// The union of the rooms' tile rectangles: the level's extent.
TileRect Bounds(const std::vector<Room>& rooms);
// Invalid when the player stands in no room of this level.
Summary Describe(const Layout& layout, Position player);
// One spoken line, in the mod's screen vocabulary.
std::string Speak(const Summary& summary, const std::string& levelName,
	const EntityRadar::Settings& scan, const Strings::Table& strings);
}
