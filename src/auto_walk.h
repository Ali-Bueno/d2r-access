#pragma once

#include "radar.h"

#include <cstdint>
#include <vector>

// Walking a proved route by giving the game one destination at a time. The
// game's own movement only finds its way over short, plain ground, so each
// order is the farthest point of the route the body can reach in a straight
// line, and the next is given as that one is neared. Pure logic over the
// collision World, unit-tested without the game; what carries the order to the
// game is the caller's business.
namespace D2RAccess::AutoWalk {
using Radar::Position;

// Navigation policy, not game constants. Distances are collision subtiles.
struct Settings {
	double reach = 20;  // the farthest an order is given from the player
	double renew = 6;   // a new order once the player is this near the last one
	uint64_t orderIntervalMs = 250; // orders are never given faster than this
	// No ground covered for this long after an order is a stall; the order is
	// given again, and after `stallOrders` of them in a row the walk is blocked.
	double stallDistance = 1;
	uint64_t stallMs = 1200;
	int stallOrders = 3;
};

// Whether the body walks from one cell to the other in a straight line.
bool Straight(const Radar::World& world, Position from, Position to);
// The farthest point of `path` within reach that the body walks straight to
// from the player; the first cell when not even that one is, so a walk always
// has somewhere to start. `path` must not be empty.
Position Leg(const Radar::World& world, const std::vector<Position>& path, const Settings& settings);

enum class Event { None, Order, Blocked };
struct Step {
	Event event = Event::None;
	Position order; // where to send the character, with Event::Order
};

class Walker {
public:
	void Start();
	void Stop() { *this = {}; }
	bool Active() const { return active_; }
	// `path` is the route from the player's cell as it is now, proved again by
	// the caller as often as it likes; an empty one gives no order.
	Step Tick(const Radar::World& world, const std::vector<Position>& path, uint64_t now, const Settings& settings);
private:
	bool active_ = false, ordered_ = false;
	Position order_{}, anchor_{}; // the last order; where the player stood when last seen moving
	uint64_t orderMs_ = 0, anchorMs_ = 0;
	int stalls_ = 0;
};
}
