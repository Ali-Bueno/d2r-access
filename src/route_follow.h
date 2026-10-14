#pragma once

#include "radar.h"
#include "route_legs.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// A described route followed on foot: the leg to walk is said as the player
// comes to each turn, and once if they leave the route. It never walks and
// never pings; it starts only when the route is asked for. Pure logic,
// unit-tested without the game. docs/route-description.md#following-it
namespace D2RAccess::RouteFollow {
using Position = Radar::Position;

enum class Event { None, Turn, Off, End };
struct Step {
	Event event = Event::None;
	size_t leg = 0; // Turn: the leg to walk now, an index into the legs given to Start
};

class Follower {
public:
	// `legs` as RouteLegs::Describe gave them for a way walked from `from`.
	void Start(Position from, const std::vector<RouteLegs::Leg>& legs);
	void Stop() { *this = {}; }
	bool Active() const { return active_; }
	const std::vector<RouteLegs::Leg>& Legs() const { return legs_; }
	// Where `player` is on the route at `now`: a turn reached, the route left
	// (then it stops), its end reached (then it stops), or nothing new.
	Step Tick(Position player, uint64_t now);
private:
	std::vector<RouteLegs::Leg> legs_;
	std::vector<Position> corners_; // corners_[i] starts legs_[i]; the last is the way's end
	size_t at_ = 0;                 // the leg being walked
	uint64_t offSince_ = 0;         // when the player was first seen off the route, 0 while on it
	bool active_ = false;
};
}
