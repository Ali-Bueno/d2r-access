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

// The part of the world a sighted player sees, the character at its middle.
// Policy, not read from the game: the legacy 800x600 view at 32 by 16 pixels
// to a subtile, which D2R's wider view is taken to contain.
struct Screen {
	double halfWidth = 25;  // |x - y| of an offset: 400 pixels either side
	double halfHeight = 37; // |x + y| of an offset: 300 pixels above and below
};
// Whether `at` is on the screen around `player`. Only what is on screen can be
// clicked, so only that is fought or used.
bool OnScreen(Position player, Position at, const Screen& screen = {});

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

// Keeping a ranged fight at range, as a player does and as the game's own
// ranged monsters do: when a monster closes, fall back to ground that puts more
// distance between, shoot from there, and fall back again. Where there is no
// such ground -- a corner, a dead end, monsters on every side -- the character
// stands and fights.
struct Spacing {
	double tooClose = 9; // a monster nearer than this is closing on the player
	double gain = 3;     // ground to fall back to is at least this much farther from every monster
	double enough = 16;  // past this, more distance is not worth a longer walk
	int leg = 14;        // the longest walk back, in walking steps
	size_t views = 48;   // how many of the best places are looked at for a view of the monster fought
};
// The distance from a place to the nearest monster; very large when there is none.
double Clearance(Position from, const std::vector<Position>& threats);
// Whether a monster has come nearer than a ranged fight allows.
bool Closing(Position player, const std::vector<Position>& threats, const Spacing& spacing = {});

struct Ground {
	bool found = false;
	Position cell;
	double clearance = 0; // how far the nearest monster is from there
};
// Where to fall back to: a cell the body walks straight to, farther from every
// monster than the player stands now, still within `range` of `target` and
// with a clear view of it, so the next shot is loosed from there. Nothing is
// found when no cell is all of that: the character is cornered.
Ground FallBack(const Radar::World& world, const Navigation::Grid& grid, const std::vector<Position>& threats,
	Position target, double range, const Spacing& spacing = {});
}
