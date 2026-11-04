#pragma once

#include "grid_jump.h"

#include <cstdint>
#include <span>
#include <vector>

// The keyboard's own cursor in a menu. In keyboard mode the game keeps no
// focused widget, so the arrows move a cursor the mod keeps: from one control
// to the nearest one that way, and inside a grid of items by item
// (grid_jump.h). A step past the edge of a grid leaves it for the nearest
// thing that way, and a step into a grid enters it at the cell nearest to
// where the step came from.
//
// The nearest thing that way is the nearest of what lies more that way than
// to one side of it, so a step to the right from the first skill of a row
// lands on the skill of the next column that is a row lower when the row has
// none there, and not on one further along the row (the user, 2026-09-27).
// Of two things that lie that way, one of the group the step is made from
// comes first: a step to the right from a skill lands on a skill, not on the
// tab drawn over the next column. A step from a thing of no group has no such
// preference: down from a tab lands on the skill under it, not the footer. With nothing like that, it is the nearest
// thing that lies that way at all. What the arrows would still miss is put
// within their reach by Mend.
//
// The rows of a list that scrolls are drawn where the scroll puts them, far off
// the screen for the ones out of view, so they are a group seen through a frame:
// a row out of view is reached only along its own list, and the keyboard's menu
// scrolls it in before the pointer goes there.
// Pure logic, unit-tested without the game. docs/keyboard-play.md
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
	uintptr_t group = 0; // things of one kind that stand together, as the skills of a skill tree
	Rect frame;          // a row of a list that scrolls: what it is seen through; no size otherwise
	bool aside = false;  // not where a menu is entered: its way out, a button of the loader's
};
struct Place {
	uintptr_t widget = 0;
	Cell anchor; // in a grid: where the thing begins, an item's top-left cell or the empty cell
	Cell travel; // in a grid: the cell travelled through (grid_jump.h)
	bool operator==(const Place&) const = default;
};
// A step that is kept instead of looked for: from one place, one way, to another.
struct Link {
	Place from, to;
	Direction direction = Direction::Left;
};

const Target* Find(std::span<const Target> targets, uintptr_t widget);
// Where the place is drawn: the control, the whole item, or the empty cell.
Rect RectOf(const Target& target, const Place& place);
// The same place after the menu changed under it: an item moved or gone, a
// grid resized. A place whose control is gone has widget 0.
Place Settle(std::span<const Target> targets, const Place& place);
// How far the middle of a row lies past its frame, in pixels: above it negative,
// below it positive, 0 in view, as anything with no frame is.
int Beyond(const Target& target);
// Where a menu is entered: the top-most thing in view, the left-most of those,
// and of the things set aside only when there is nothing else.
Place First(std::span<const Target> targets);
// `byCell` steps a grid one cell at a time, as putting an item down needs.
// The place itself is returned when nothing lies that way. A link from the
// place that way is the step, the last one made first.
Place Move(std::span<const Target> targets, const Place& from, Direction direction, bool byCell, std::span<const Link> links = {});
// Every place there is, and every place the arrows reach from `from`, each
// given once by its anchor: what the second lacks of the first cannot be
// reached, and is logged by the caller.
std::vector<Place> Everything(std::span<const Target> targets);
std::vector<Place> Reachable(std::span<const Target> targets, const Place& from, std::span<const Link> links = {});
// The links that put every place within reach of the arrows from every other,
// none when they reach everything as it is. A place missed from where the menu
// is entered is put into the step of the nearest place that is reached: that
// step lands on it, the same step again lands where it landed before, and the
// step back returns. A place that does not lead back to where the menu is
// entered is given a step to the nearest place that does.
std::vector<Link> Mend(std::span<const Target> targets);
}
