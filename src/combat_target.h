#pragma once

#include "entity_radar.h"
#include "navigation_grid.h"
#include "radar.h"

#include <cstdint>
#include <vector>

// Which monster the fighting keys mean. A sighted player points at one with the
// mouse; here the nearest is taken, the player steps through the others by
// name, and a fight stays on the monster it began with until that one is gone.
// Pure logic over the entities and the reachability grid the scanner already
// reads, so it is unit-tested without the game.
namespace D2RAccess::CombatTarget {
using Radar::Position;

struct Candidate {
	EntityRadar::Entity entity;
	double distance = 0; // straight line, in subtiles
	int steps = -1;      // walking steps; -1 when no way to it is known
	bool sight = false;  // nothing stands between the player and it
};

// Navigation policy, not game constants. Distances are collision subtiles.
struct Settings {
	double range = 60; // a monster farther off than this is not offered
};

// The living enemies among `entities` within range, nearest first: by walking
// steps, so a monster behind a wall counts as far as the way round to it, and
// the ones with no known way to them last, by straight line.
std::vector<Candidate> Rank(const Radar::World& world, const Navigation::Grid& grid,
	const std::vector<EntityRadar::Entity>& entities, const Settings& settings = {});

class Lock {
public:
	uint32_t Id() const { return id_; }
	bool Held() const { return id_ != 0; }
	void Clear() { id_ = 0; }
	// The player's own choice. Zero takes the nearest; +1 and -1 step to the
	// next farther and the next nearer than the one held, wrapping, and take the
	// nearest when none is held or the held one is gone. Null when `ranked` is empty.
	const Candidate* Choose(const std::vector<Candidate>& ranked, int delta);
	// While a fight goes on: the held monster for as long as it is listed, the
	// nearest once it is not. `moved` tells a new monster from the same one, so
	// each is named once. Null, and nothing held, when `ranked` is empty.
	const Candidate* Keep(const std::vector<Candidate>& ranked, bool& moved);
private:
	uint32_t id_ = 0;
};

// Rangefinding: whether to strike from here or close the distance first.
struct Reach {
	bool ranged = false;
	double range = 0; // how far the attack carries, in subtiles; a ranged attack also needs sight
};
enum class Move { Strike, Close, Unreachable };
Move Decide(const Candidate& target, const Reach& reach);
}
