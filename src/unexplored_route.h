#pragma once

#include "explored_map.h"
#include "navigation_grid.h"
#include "radar.h"
#include "speech_strings.h"

#include <optional>
#include <string>
#include <vector>

// The way to the nearest place the player has not been, spoken as walking
// instructions: "nearest unexplored space, up 6, right 4, up 3".
//
// The steps are the four fixed screen directions the left stick already uses,
// so each one is a push the player can hold; in world coordinates they are the
// diagonals, which is how D2R's own movement is laid out. The search runs over
// the navigation grid the sonar builds (Navigation::Build), so it obeys exactly
// the walls that grid knows, and over Explored::Map, which is the mod's own
// memory of where the player has walked.
namespace D2RAccess::UnexploredRoute {
struct Settings {
	// Half-width of the analysis window in subtiles, passed to Navigation::Build.
	// The world snapshot reaches 152 subtiles; this is the distance a route may
	// cover before the answer is "nothing unexplored nearby".
	int radius = 80;
	// Direction runs spoken at once. A long route is given as its first runs;
	// pressing the key again after walking them gives the next ones, which is
	// the step-by-step readout the feature is for.
	size_t maxRuns = 6;
	// How deep inside unexplored ground a target must sit, in subtiles, for it
	// to be worth walking to. Without this the target is the first cell the
	// memory does not cover — the rim of the brush the player carries with him,
	// which retreats exactly as fast as he approaches it, so the readout never
	// counts down. Keep it at least the brush's own radius.
	int probeRadius = 12;
};

// What the caller already knows, so consecutive presses agree with each other.
struct Request {
	// The target of the previous press. It stays the target until it has been
	// explored (arriving does that) or can no longer be reached.
	std::optional<Radar::Position> locked;
};

// One run of identical steps: "up 6" means six pushes of the stick screen-up.
struct Step {
	Radar::Direction direction = Radar::Direction::Up;
	int count = 0;
};

enum class Outcome {
	Unavailable, // no usable grid: world unread, player outside it
	None,        // everything reachable around the player is already explored
	Route,       // walkable step-by-step route
	Bearing,     // target found, but no route along the four screen directions
};

struct Route {
	Outcome outcome = Outcome::Unavailable;
	Radar::Position target;   // world subtiles
	int distance = 0;         // steps along the route, or of the straight bearing
	bool truncated = false;   // more runs follow the ones in steps
	bool keptLocked = false;  // the previous press's target, still being walked to
	bool arrived = false;     // the previous press's target has been explored
	std::vector<Step> steps;
};

Route Find(const Navigation::Grid& grid, const Explored::Map& explored, const Settings& settings = {},
	const Request& request = {});

// "Nearest unexplored space, up 6, right 4"; the phrases are the movement
// directions the radar already speaks.
std::string Describe(const Route& route, const Strings::Table& strings);
}
