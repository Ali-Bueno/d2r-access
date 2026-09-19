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
	// Distances are collision subtiles, the unit the entity scan speaks as
	// metres. Navigation policy, not game constants.
	//
	// The search box is half-sided by searchRadius: as wide as the game keeps
	// collision around the player (native_world.cpp holds a 152-subtile
	// window) plus the look-ahead, so anything the level-wide catalogue can
	// name can also be routed to. The box is still cut to the target's actual
	// distance, so a near target searches a small one and pays a small cost.
	double searchRadius = 158;
	double lookAhead = 8;       // route length that chooses the heading
	double arrivalDistance = 2; // within this the target counts as reached
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
// room is not loaded, the route heads for the reachable cell nearest to it,
// which is what walking toward an unseen destination means in practice.
Route FindRoute(const Radar::World& world, Position target, const Settings& settings);
uint64_t PingIntervalMs(double remaining, const Settings& settings);

struct Update {
	bool valid = false;   // a target is locked and its position was supplied
	bool ping = false;    // play one ping this tick
	bool arrived = false; // the target is within arrivalDistance
	Position heading{};   // unit vector; the straight line when no route is known
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
	Update Tick(const Radar::World& world, Position target, uint64_t now, const Settings& settings);
	const Route& CurrentRoute() const { return route_; }
private:
	bool locked_ = false;
	Route route_;
	Position routeOrigin_{}, routeTarget_{};
	bool routed_ = false;
	uint64_t routeMs_ = 0, nextPingMs_ = 0;
	bool first_ = false; // the first update after a lock waits for the spoken name
};
}
