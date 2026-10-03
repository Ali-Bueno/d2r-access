#pragma once

#include <vector>

// Moving through a grid of items by item instead of by cell: a step lands on
// the next thing in that direction, an item or an empty cell, and passes over
// the rest of the cells the item it left covers. An empty cell is still one
// step, since where the room is matters when placing something.
//
// The cursor keeps the line it travels along. Stepping right from a tall item
// goes on along the row it was entered on, not along the item's top row, so a
// run of steps one way meets everything that touches that row or column, and
// nothing is passed by. Pure logic, unit-tested without the game.
namespace D2RAccess::GridJump {
struct Item { int x = 0, y = 0, width = 1, height = 1; }; // in cells, top-left first
struct Grid {
	int width = 0, height = 0;
	std::vector<Item> items;
};
struct Cell {
	int x = 0, y = 0;
	bool operator==(const Cell&) const = default;
};
enum class Direction { Left, Right, Up, Down };

inline constexpr int Empty = -1;
// Index into the grid's items of the one covering this cell, or Empty.
int ItemAt(const Grid& grid, Cell cell);

struct Step {
	bool moved = false; // false at the grid's edge: there is nothing further that way
	Cell cursor;        // the cell travelled through; pass it to the next Move
	int item = Empty;   // what the step landed on
	Cell anchor;        // where that thing begins: the item's top-left cell, or the empty cell itself
};
// `cursor` must lie inside the grid.
Step Move(const Grid& grid, Cell cursor, Direction direction);
}
