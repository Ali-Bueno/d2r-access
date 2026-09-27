#pragma once

#include "grid_jump.h"

#include <cstdint>
#include <span>
#include <vector>

// The keyboard's own cursor in a menu. In keyboard mode the game keeps no
// focused widget, so the arrows move a cursor the mod keeps: from one control
// to the next by where they are drawn (menu_navigation_policy.h), and inside a
// grid of items by item (grid_jump.h). A step past the edge of a grid leaves
// it for the nearest thing that way, and a step into a grid enters it at the
// cell nearest to where the step came from. Pure logic, unit-tested without
// the game. docs/keyboard-play.md
namespace D2RAccess::MenuCursor {
using GridJump::Cell;
using GridJump::Direction;

struct Rect {
	int x = 0, y = 0, width = 0, height = 0;
	bool operator==(const Rect&) const = default;
};
// One thing the arrows can be on: a control, or a grid and everything in it.
struct Target {
	uintptr_t widget = 0;
	Rect rect;           // where it is drawn; a grid's holds all of its cells
	GridJump::Grid grid; // width 0 for a control, which is one place
};
struct Place {
	uintptr_t widget = 0;
	Cell anchor; // in a grid: where the thing begins, an item's top-left cell or the empty cell
	Cell travel; // in a grid: the cell travelled through (grid_jump.h)
	bool operator==(const Place&) const = default;
};

const Target* Find(std::span<const Target> targets, uintptr_t widget);
// Where the place is drawn: the control, the whole item, or the empty cell.
Rect RectOf(const Target& target, const Place& place);
// The same place after the menu changed under it: an item moved or gone, a
// grid resized. A place whose control is gone has widget 0.
Place Settle(std::span<const Target> targets, const Place& place);
// Where a menu is entered: the top-most thing, the left-most of those.
Place First(std::span<const Target> targets);
// `byCell` steps a grid one cell at a time, as putting an item down needs.
// The place itself is returned when nothing lies that way.
Place Move(std::span<const Target> targets, const Place& from, Direction direction, bool byCell);
// Every place there is, and every place the arrows reach from `from`, each
// given once by its anchor: what the second lacks of the first cannot be
// reached, and is logged by the caller.
std::vector<Place> Everything(std::span<const Target> targets);
std::vector<Place> Reachable(std::span<const Target> targets, const Place& from);
}
