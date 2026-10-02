#pragma once

#include "native_rooms.h"
#include "navigation_grid.h"
#include "pad_links.h"
#include "radar.h"

#include <cstdint>
#include <optional>
#include <vector>

// The floor of a level as the game has shown it so far, kept when the game
// frees it again, and the way to a place over it.
//
// The game keeps collision only around the player (native_world.cpp), so what
// a walk knows of the level changes as it goes. 2026-10-01, the Far Oasis: a
// walk to the Maggot Lair turned round every few seconds, for minutes. With the
// lair's surroundings loaded the route was the true one, some 230 steps back
// out and round a dune; two rooms further off they were not loaded, and the
// room graph's guess led straight at the lair again, into the dead end under
// the dune. Each crossing of a room border changed the plan.
//
// A way found here crosses only ground seen open while such a way exists.
// Failing that it may cross ground not seen yet, which is what walking toward
// an unseen place means. It never crosses ground seen shut. What was seen
// stays seen, so a look can only rule ways out, and a walk cannot go back and
// forth between two of them. Pure logic, unit-tested without the game.
namespace D2RAccess::LevelRoute {
using Radar::Position;

// Door: a shut door, and nothing else in the cell that stops a walk. No way is
// planned through one, since the game will not walk through it; but a door can
// be opened, so what lies past one is not cut off (Reach).
enum class Ground : uint8_t { Outside, Unseen, Open, Blocked, Door };

class Memory {
public:
	// The level's logical rooms bound it: ground inside one is unseen until the
	// game has loaded it, and ground outside them all is no part of the level.
	// False, and nothing kept, when the rooms are none or too many cells.
	bool Begin(uint64_t session, int level, const std::vector<NativeRooms::Room>& rooms);
	// Copies in the rooms the game has loaded now. True when a cell changed.
	bool Learn(const Radar::World& world);
	Ground At(int x, int y) const;
	const std::vector<Ground>& Cells() const { return cells_; } // Width() to a row, from X(), Y()
	uint64_t Session() const { return session_; }
	int Level() const { return level_; }
	uint64_t Revision() const { return revision_; } // one more for every Learn that changed a cell
	int X() const { return x_; }
	int Y() const { return y_; }
	int Width() const { return width_; }
	int Height() const { return height_; }
private:
	uint64_t session_ = 0, revision_ = 0;
	int level_ = 0, x_ = 0, y_ = 0, width_ = 0, height_ = 0;
	std::vector<Ground> cells_;
};

// The steps to a place from every cell that has a way to it. A cell is stood on
// when the whole body fits there, as the game's own footprint test has it
// (pattern 0 a point, 1/3/5 the five-cell cross, 2/4 the 3x3 square).
class Field {
public:
	// `margin`: the cells round the place that count as reaching it, since a
	// thing's own cells carry its collision. `unseen` lets the way cross ground
	// not seen yet.
	Field(const Memory& memory, Position place, int margin, int bodyPattern, bool unseen);
	// The cells to walk from `from`, the last of them at the place. Empty when
	// there is no way, and when `from` is at the place already. The cell stood
	// on need not fit the body: the game never tests the cell a move starts from.
	std::vector<Position> From(Position from) const;
	bool At(Position from) const; // `from` is at the place
private:
	int Steps(int x, int y) const;
	int x_ = 0, y_ = 0, width_ = 0, height_ = 0;
	std::vector<int32_t> steps_; // -1: no way from here
};

struct Way {
	std::vector<Position> path; // the cells to walk, the last of them at the place; empty when there is no way
	bool guess = false;         // part of it is ground not seen yet
};

// The ways to one place, looked up again only when the map or the place changes:
// a search of the whole level is too much to make five times a second.
class Plan {
public:
	Way From(const Memory& memory, Position from, Position place, int margin, int bodyPattern);
	void Clear() { *this = {}; }
private:
	struct Key {
		uint64_t session = 0, revision = 0;
		int level = 0, x = 0, y = 0, margin = 0, pattern = 0;
		bool operator==(const Key&) const = default;
	};
	Key key_;
	std::optional<Field> seen_, any_;
};

// The ground the player can get to on foot from where they stand, as far as the
// game has shown the level: over ground seen open, over ground not seen yet,
// and through doors, which can be opened. What it does not reach is cut off by
// ground seen shut all the way round, and only that is called cut off: a place
// past ground not seen yet may still have a way.
//
// 2026-10-02, the Arcane Sanctuary: its walkways are joined by teleport pads
// alone. The scanner listed every pad loaded, twenty of them, and 34 walks to
// a pad ended with no way, one of them only after running for four seconds to
// the edge nearest the pad.
class Reach {
public:
	enum class Answer { Unknown, Reached, CutOff };
	Reach() = default;
	Reach(const Memory& memory, Position from, int bodyPattern);
	// The same search, through the teleport pads in `links` too (pad_links.h):
	// ground got to within `useWithin` cells of a pad, as near as D uses one
	// from, gets to the ground within LandWithin of the pad it leads to. A pad
	// counts as one step. `pads` is the Pads::Revision the links were taken at.
	Reach(const Memory& memory, Position from, int bodyPattern, const std::vector<Link>& links, int useWithin, uint64_t pads);
	// Whether this is still the ground reached from `from` on `memory` as it is
	// now: the same map, unchanged, and `from` on the ground reached.
	bool Holds(const Memory& memory, Position from, int bodyPattern) const;
	// The search through pads holds only from the cell it was made from, with
	// the same pads: which pad comes first depends on where the way starts.
	bool Holds(const Memory& memory, Position from, int bodyPattern, uint64_t pads) const;
	// A thing: reached when the body stands on reached ground within `margin`
	// cells of it. Unknown when none of those cells is part of the level, and
	// when the player stands nowhere the body fits.
	Answer To(Position place, int margin) const;
	// A room, max exclusive: reached when any of its cells is.
	Answer Within(const NativeRooms::Bounds& bounds) const;
	// The pad to use first on the shortest way to a thing or a room the search
	// through pads reached. False when the way takes no pad, or there is none.
	bool Via(Position place, int margin, Pad& pad) const;
	bool ViaWithin(const NativeRooms::Bounds& bounds, Pad& pad) const;
private:
	// Outside the level; no place for the body; a place for it, not got to; got to.
	enum Mark : uint8_t { Outside, Shut, Stood, Got };
	void Flood(const Memory& memory, Position from, int bodyPattern, const std::vector<Link>& links, int useWithin);
	Mark At(int x, int y) const;
	Answer Among(int minX, int minY, int maxX, int maxY) const; // max exclusive
	bool First(int minX, int minY, int maxX, int maxY, Pad& pad) const; // max exclusive
	uint64_t session_ = 0, revision_ = 0, pads_ = 0;
	int level_ = 0, pattern_ = 0, x_ = 0, y_ = 0, width_ = 0, height_ = 0, fromX_ = 0, fromY_ = 0;
	bool any_ = false, linked_ = false;
	std::vector<uint8_t> cells_; // one cell of margin all round, as Field has
	// With pads only: the steps to each cell got to, and the first pad of that
	// way as an index into links_ plus one, zero when walked to.
	std::vector<int32_t> steps_;
	std::vector<uint16_t> via_;
	std::vector<Link> links_;
};

// Where a walk along `path` heads now: its last cell before it leaves the floor
// the game has loaded and the player can reach. From there on the game has
// nothing for a move to be planned on; walking up to it is what loads the rest.
// False when not even the first cell is reached.
bool Ahead(const std::vector<Position>& path, const Navigation::Grid& grid, Position& cell);
}
