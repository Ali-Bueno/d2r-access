#pragma once

#include "radar.h"

#include <cstdint>
#include <vector>

// A locked target (a level exit, an NPC, a chest, a monster) is followed with
// a ping that speeds up as the remaining route shortens. Its direction is the
// next leg of a route proved on the game's own collision map, so a dungeon
// corridor is followed and corners are turned instead of pointing through a
// wall. Pure logic over the collision World the sonar already reads, so it is
// unit-tested without the game.
namespace D2RAccess::Beacon {
using Radar::Position;

struct Settings {
	Radar::Layer layer = Radar::Layer::Terrain; // movement commands may also avoid live unit footprints
	bool requireKnownRoute = false; // exploration mode ends a lock left without a proved route for routeLossMs
	// Distances are collision subtiles, the unit the scanner speaks as
	// steps. Navigation policy, not game constants.
	//
	// The search box is half-sided by searchRadius: as wide as the game keeps
	// collision around the player (native_world.cpp holds a 152-subtile
	// window) plus the look-ahead, so anything the level-wide catalogue can
	// name can also be routed to. The box is still cut to the target's actual
	// distance, so a near target searches a small one and pays a small cost.
	double searchRadius = 158;
	double lookAhead = 8;       // route length that chooses the heading
	// Within this the target counts as reached: the keyboard's use reach. Two bodies
	// side by side stand sqrt(5) apart, so a smaller one never sees an NPC reached.
	double arrivalDistance = 3;
	// How stale a route may get before it is proved again. Both are stretched
	// in proportion once the target is further off than farDistance: the box
	// grows with the distance, while a long route changes more slowly, so
	// without that a level-wide lead would run the widest search several times
	// a second for a heading that barely moves.
	double reroute = 3;         // player/target movement that invalidates a route
	uint64_t routeIntervalMs = 400; // otherwise the route is recomputed this often
	// Ping cadence, linear in the remaining route length between these bounds.
	uint64_t nearPingMs = 160, farPingMs = 1200;
	double farDistance = 40; // remaining route at which the cadence is slowest
	// Room for the spoken target name before the first ping.
	uint64_t lockDelayMs = 600;
	// How long a known route may stay unproved, silently, before exploration
	// ends the lock: a step across a room boundary or a short read gap must not.
	uint64_t routeLossMs = 3000;
};

struct Route {
	bool known = false;     // a first leg was proved on the collision map
	bool reachable = false; // the search reached the target itself
	Position heading{};     // unit vector in world cells
	double length = 0;      // route length to the sub-goal, in cells
	std::vector<Position> path; // player cell -> sub-goal cell
};

// Bounded route search on the terrain layer: monsters and players standing in
// a doorway do not reroute the player. When the target is unreachable or its
// room is not loaded, the route heads for the reachable cell nearest to it; a
// walk may take that, guidance only when it Reaches the target.
Route FindRoute(const Radar::World& world, Position target, const Settings& settings);
// Whether the player or the route's end gets within interaction distance of `target`;
// an object can occupy its own destination cell.
bool Reaches(const Radar::World& world, const Route& route, Position target, const Settings& settings);
uint64_t PingIntervalMs(double remaining, const Settings& settings);

struct Update {
	bool valid = false;   // a target is locked and its position was supplied
	bool ping = false;    // play one ping this tick
	bool arrived = false; // the target is within arrivalDistance
	bool routeUnavailable = false; // exploration mode has not proved a route for Settings::routeLossMs
	Position heading{};   // unit vector along the proved route; zero, and no ping, when none is known
	bool routed = false;  // the heading follows a proved route
	double distance = 0;  // straight-line distance to the target
	double remaining = 0; // route length when routed, straight distance otherwise
};

class Tracker {
public:
	void Lock(uint64_t now);
	void Clear();
	bool Locked() const { return locked_; }
	// `target` is the target's current position, re-read by the caller; the
	// player's position comes from the same snapshot as the collision.
	//
	// `waypoint`, when supplied, is where the ROUTE leads instead -- a cell
	// the caller has already proved reachable, typically as far along the
	// remembered map's way as the loaded floor goes (beacon_navigation.h), for
	// a target the collision window cannot reach. Distance, arrival and ping
	// cadence stay measured against `target`, so the lock itself is unchanged:
	// only the heading follows the waypoint.
	Update Tick(const Radar::World& world, Position target, uint64_t now, const Settings& settings,
		const Position* waypoint = nullptr);
	const Route& CurrentRoute() const { return route_; }
	// Notes that no known route exists now; true once none has for routeLossMs.
	// Any update that proves the route again restarts the count.
	bool RouteLost(uint64_t now, const Settings& settings);
private:
	bool locked_ = false;
	bool lost_ = false;
	uint64_t lostMs_ = 0;
	Route route_;
	Position routeOrigin_{}, routeTarget_{};
	bool routed_ = false;
	uint64_t routeMs_ = 0, nextPingMs_ = 0;
	bool first_ = false; // the first update after a lock waits for the spoken name
};
}
