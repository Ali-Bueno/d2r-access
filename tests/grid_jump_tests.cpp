#include "grid_jump.h"
#include "test_support.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

using namespace D2RAccess;
using GridJump::Cell;
using GridJump::Direction;
using GridJump::Grid;
using GridJump::Item;

namespace {
constexpr Direction All[] = {Direction::Left, Direction::Right, Direction::Up, Direction::Down};

// Everything in the grid as one number each: an item by its index, an empty
// cell by its position, so an empty cell counts as a thing of its own.
int Thing(const Grid& grid, Cell cell) {
	const int item = GridJump::ItemAt(grid, cell);
	return item != GridJump::Empty ? item : 1000 + cell.y * grid.width + cell.x;
}
std::set<int> Everything(const Grid& grid) {
	std::set<int> things;
	for (int y = 0; y < grid.height; ++y)
		for (int x = 0; x < grid.width; ++x) things.insert(Thing(grid, {x, y}));
	return things;
}
// Everything a run of steps one way meets, from a cell at the near edge.
std::set<int> Run(const Grid& grid, Cell start, Direction direction) {
	std::set<int> met{Thing(grid, start)};
	Cell cursor = start;
	for (int guard = 0; guard < grid.width * grid.height + 1; ++guard) {
		const auto step = GridJump::Move(grid, cursor, direction);
		if (!step.moved) break;
		cursor = step.cursor;
		met.insert(Thing(grid, cursor));
	}
	return met;
}
// Everything reachable from a cell by any steps at all.
std::set<int> Reachable(const Grid& grid, Cell start) {
	std::set<int> met{Thing(grid, start)};
	std::vector<Cell> queue{start};
	std::vector<uint8_t> seen(static_cast<size_t>(grid.width) * grid.height, 0);
	seen[static_cast<size_t>(start.y) * grid.width + start.x] = 1;
	for (size_t head = 0; head < queue.size(); ++head) {
		for (const auto direction : All) {
			const auto step = GridJump::Move(grid, queue[head], direction);
			if (!step.moved) continue;
			auto& mark = seen[static_cast<size_t>(step.cursor.y) * grid.width + step.cursor.x];
			if (mark) continue;
			mark = 1;
			met.insert(Thing(grid, step.cursor));
			queue.push_back(step.cursor);
		}
	}
	return met;
}

// A grid filled at random with items up to 2 by 4, the largest the game has,
// leaving some cells empty. A fixed generator, so a failure can be repeated.
Grid Random(uint32_t& seed, int width, int height) {
	const auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
	Grid grid{width, height, {}};
	std::vector<uint8_t> taken(static_cast<size_t>(width) * height, 0);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (taken[static_cast<size_t>(y) * width + x] || next() % 3 == 0) continue;
			int w = 1 + static_cast<int>(next() % 2), h = 1 + static_cast<int>(next() % 4);
			w = std::min(w, width - x);
			h = std::min(h, height - y);
			bool fits = true;
			for (int dy = 0; dy < h && fits; ++dy)
				for (int dx = 0; dx < w && fits; ++dx) fits = !taken[static_cast<size_t>(y + dy) * width + x + dx];
			if (!fits) { w = h = 1; }
			for (int dy = 0; dy < h; ++dy)
				for (int dx = 0; dx < w; ++dx) taken[static_cast<size_t>(y + dy) * width + x + dx] = 1;
			grid.items.push_back({x, y, w, h});
		}
	}
	return grid;
}
}

int main() {
	{
		// The user's own example: a two by two armour beside the cursor.
		// Columns 0..9, rows 0..3, as the inventory; armour at columns 1-2, rows 0-1.
		const Grid grid{10, 4, {{1, 0, 2, 2}, {3, 0, 1, 3}}};
		const auto onto = GridJump::Move(grid, {0, 0}, Direction::Right);
		Check(onto.moved && onto.item == 0, "a step right from beside the armour lands on the armour");
		Check(onto.anchor == Cell{1, 0}, "the armour is given by its top-left cell");
		const auto past = GridJump::Move(grid, onto.cursor, Direction::Right);
		Check(past.moved && past.item == 1 && past.anchor == Cell{3, 0}, "the next step passes over the armour's other cells to the next item");
		const auto empty = GridJump::Move(grid, past.cursor, Direction::Right);
		Check(empty.moved && empty.item == GridJump::Empty && empty.anchor == Cell{4, 0}, "an empty cell is a step of its own");
		const auto back = GridJump::Move(grid, empty.cursor, Direction::Left);
		Check(back.item == 1, "a step back lands on the item just left");
		const auto below = GridJump::Move(grid, onto.cursor, Direction::Down);
		Check(below.moved && below.item == GridJump::Empty && below.anchor == Cell{1, 2}, "a step down passes over the armour's lower row");
	}
	{
		// The row travelled along is kept: entering a tall item on its second
		// row and going on leaves it along that same row.
		const Grid grid{4, 3, {{1, 0, 1, 3}, {2, 1, 1, 1}, {2, 0, 1, 1}}};
		const auto onto = GridJump::Move(grid, {0, 1}, Direction::Right);
		Check(onto.item == 0 && onto.anchor == Cell{1, 0}, "a tall item entered low is still given by its top-left cell");
		const auto on = GridJump::Move(grid, onto.cursor, Direction::Right);
		Check(on.item == 1, "going on from it keeps to the row it was entered on");
	}
	{
		const Grid grid{3, 2, {{0, 0, 1, 2}}};
		Check(!GridJump::Move(grid, {0, 0}, Direction::Left).moved, "nothing lies past the left edge");
		Check(!GridJump::Move(grid, {0, 0}, Direction::Up).moved, "nothing lies past the top edge");
		Check(!GridJump::Move(grid, {0, 0}, Direction::Down).moved, "an item reaching the bottom edge has nothing below it");
		Check(!GridJump::Move(grid, {2, 1}, Direction::Right).moved, "nothing lies past the right edge");
		Check(GridJump::Move(grid, {0, 0}, Direction::Left).item == 0, "a refused step still names what the cursor stands on");
	}
	{
		// Nothing is ever passed by, on a thousand grids of every shape the
		// game has: inventory, stash, cube and belt sizes.
		uint32_t seed = 20260927;
		const int sizes[][2] = {{10, 4}, {10, 10}, {3, 4}, {4, 4}, {16, 13}, {1, 1}, {2, 8}};
		bool rows = true, columns = true, reachable = true, anchored = true;
		for (int round = 0; round < 1000; ++round) {
			const auto& size = sizes[round % 7];
			const Grid grid = Random(seed, size[0], size[1]);
			const auto all = Everything(grid);
			for (int y = 0; y < grid.height; ++y) {
				std::set<int> inRow;
				for (int x = 0; x < grid.width; ++x) inRow.insert(Thing(grid, {x, y}));
				rows &= Run(grid, {0, y}, Direction::Right) == inRow;
				rows &= Run(grid, {grid.width - 1, y}, Direction::Left) == inRow;
			}
			for (int x = 0; x < grid.width; ++x) {
				std::set<int> inColumn;
				for (int y = 0; y < grid.height; ++y) inColumn.insert(Thing(grid, {x, y}));
				columns &= Run(grid, {x, 0}, Direction::Down) == inColumn;
				columns &= Run(grid, {x, grid.height - 1}, Direction::Up) == inColumn;
			}
			for (int y = 0; y < grid.height; ++y) {
				for (int x = 0; x < grid.width; ++x) {
					reachable &= Reachable(grid, {x, y}) == all;
					for (const auto direction : All) {
						const auto step = GridJump::Move(grid, {x, y}, direction);
						anchored &= GridJump::ItemAt(grid, step.anchor) == step.item
							&& Thing(grid, step.anchor) == Thing(grid, step.cursor);
					}
				}
			}
		}
		Check(rows, "a run along a row meets everything that touches the row, both ways");
		Check(columns, "a run along a column meets everything that touches the column, both ways");
		Check(reachable, "everything in a grid can be reached from anywhere in it");
		Check(anchored, "what a step lands on is always given by a cell of its own");
	}
	Passed();
	return 0;
}
