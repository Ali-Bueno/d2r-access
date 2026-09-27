#include "menu_cursor.h"
#include "test_support.h"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace D2RAccess;
using MenuCursor::Direction;
using MenuCursor::Place;
using MenuCursor::Rect;
using MenuCursor::Target;

namespace {
// A grid filled at random with items up to 2 by 4, the largest the game has.
GridJump::Grid Random(uint32_t& seed, int width, int height) {
	const auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
	GridJump::Grid grid{width, height, {}};
	std::vector<uint8_t> taken(static_cast<size_t>(width) * height, 0);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (taken[static_cast<size_t>(y) * width + x] || next() % 3 == 0) continue;
			int w = std::min(1 + static_cast<int>(next() % 2), width - x), h = std::min(1 + static_cast<int>(next() % 4), height - y);
			bool fits = true;
			for (int dy = 0; dy < h && fits; ++dy)
				for (int dx = 0; dx < w && fits; ++dx) fits = !taken[static_cast<size_t>(y + dy) * width + x + dx];
			if (!fits) w = h = 1;
			for (int dy = 0; dy < h; ++dy)
				for (int dx = 0; dx < w; ++dx) taken[static_cast<size_t>(y + dy) * width + x + dx] = 1;
			grid.items.push_back({x, y, w, h});
		}
	}
	return grid;
}

// The shape of the inventory: equipment around a figure, a grid of items
// under it, the gold and a close button at the foot. Cells are 100 across.
enum : uintptr_t { Head = 1, Amulet, Right, Torso, Left, RingRight, Belt, RingLeft, Gloves, Boots, Items, Gold, Close, Stash };
std::vector<Target> Inventory(GridJump::Grid items) {
	return {
		{Head, {450, 0, 200, 200}, {}}, {Amulet, {700, 50, 100, 100}, {}},
		{Right, {100, 100, 200, 400}, {}}, {Torso, {450, 250, 200, 300}, {}}, {Left, {800, 100, 200, 400}, {}},
		{RingRight, {330, 600, 100, 100}, {}}, {Belt, {450, 600, 200, 100}, {}}, {RingLeft, {670, 600, 100, 100}, {}},
		{Gloves, {100, 550, 200, 200}, {}}, {Boots, {800, 550, 200, 200}, {}},
		{Items, {50, 800, 1000, 400}, std::move(items)},
		{Gold, {100, 1250, 300, 60}, {}}, {Close, {900, 1250, 100, 60}, {}},
	};
}

bool AllReached(const std::vector<Target>& targets) {
	const auto all = MenuCursor::Everything(targets);
	for (const auto& from : all) {
		const auto reached = MenuCursor::Reachable(targets, from);
		if (reached.size() != all.size()) return false;
		for (const auto& place : all)
			if (std::find(reached.begin(), reached.end(), place) == reached.end()) return false;
	}
	return true;
}
}

int main() {
	{
		// Columns 0..9, rows 0..3: an armour at columns 1-2, rows 0-1, a spear at column 3, rows 0-2.
		const auto targets = Inventory({10, 4, {{1, 0, 2, 2}, {3, 0, 1, 3}}});
		Check(MenuCursor::First(targets) == Place{Head, {}, {}}, "a menu is entered at its top-most control");
		const auto* grid = MenuCursor::Find(targets, Items);
		Check(grid && MenuCursor::RectOf(*grid, {Items, {1, 0}, {2, 1}}) == Rect{150, 800, 200, 200}, "an item is drawn over all of its cells");
		Check(MenuCursor::RectOf(*grid, {Items, {9, 3}, {9, 3}}) == Rect{950, 1100, 100, 100}, "an empty cell is drawn as itself");
		Check(MenuCursor::RectOf(targets[0], {Head, {}, {}}) == Rect{450, 0, 200, 200}, "a control is drawn as itself");

		const auto onto = MenuCursor::Move(targets, {Items, {0, 0}, {0, 0}}, Direction::Right, false);
		Check(onto == Place{Items, {1, 0}, {1, 0}}, "inside a grid a step lands on the next item");
		const auto past = MenuCursor::Move(targets, onto, Direction::Right, false);
		Check(past == Place{Items, {3, 0}, {3, 0}}, "and the next passes over the rest of its cells");
		const auto cell = MenuCursor::Move(targets, {Items, {0, 0}, {0, 0}}, Direction::Right, true);
		Check(cell == Place{Items, {1, 0}, {1, 0}}, "with an item in hand a step is one cell");
		Check(MenuCursor::Move(targets, cell, Direction::Right, true) == Place{Items, {2, 0}, {2, 0}}, "and the next is the next cell, item or no item");

		const auto up = MenuCursor::Move(targets, {Items, {1, 0}, {2, 0}}, Direction::Up, false);
		Check(up.widget == Gloves, "a step up from the top row leaves the grid for what is above it");
		const auto down = MenuCursor::Move(targets, {Gloves, {}, {}}, Direction::Down, false);
		Check(down.widget == Items && down.anchor == GridJump::Cell{1, 0}, "a step down into the grid lands on what stands under the control");
		Check(down.travel.y == 0 && down.travel.x >= 1 && down.travel.x <= 2, "and enters it on the top row");
		const auto foot = MenuCursor::Move(targets, {Items, {3, 0}, {3, 2}}, Direction::Down, false);
		Check(foot == Place{Items, {3, 3}, {3, 3}}, "a step down from a tall item lands under it");
		Check(MenuCursor::Move(targets, foot, Direction::Down, false).widget == Gold, "a step down from the bottom row leaves the grid");
		const auto back = MenuCursor::Move(targets, {Close, {}, {}}, Direction::Up, false);
		Check(back == Place{Items, {9, 3}, {9, 3}}, "a step up into the grid enters it on the bottom row, above the control");
		Check(MenuCursor::Move(targets, {Head, {}, {}}, Direction::Up, false) == Place{Head, {}, {}}, "nothing lies past the top of the menu");
		Check(MenuCursor::Move(targets, {99, {}, {}}, Direction::Down, false) == MenuCursor::First(targets), "a cursor on nothing enters the menu");
	}
	{
		auto targets = Inventory({10, 4, {{1, 0, 2, 2}}});
		const Place on{Items, {1, 0}, {2, 1}};
		Check(MenuCursor::Settle(targets, on) == on, "a place that still stands is kept");
		targets[10].grid.items.clear();
		Check(MenuCursor::Settle(targets, on) == Place{Items, {2, 1}, {2, 1}}, "with the item gone the cursor is on the cell it stood on");
		targets[10].grid = {3, 1, {}};
		Check(MenuCursor::Settle(targets, on) == Place{Items, {2, 0}, {2, 0}}, "in a smaller grid it is on the nearest cell there is");
		targets.erase(targets.begin() + 10);
		Check(MenuCursor::Settle(targets, on).widget == 0, "with the grid gone the cursor is on nothing");
	}
	{
		// Two grids side by side, as the inventory beside the stash.
		uint32_t seed = 20260927;
		bool reached = true;
		for (int round = 0; round < 60; ++round) {
			auto targets = Inventory(Random(seed, 10, 4));
			targets.push_back({Stash, {1200, 200, 1000, 1000}, Random(seed, 10, 10)});
			reached &= AllReached(targets);
		}
		Check(reached, "every control, item and empty cell is reached from every other, in sixty menus filled at random");
		const auto targets = [&] { auto made = Inventory({10, 4, {}}); made.push_back({Stash, {1200, 200, 1000, 1000}, {10, 10, {}}}); return made; }();
		const auto across = MenuCursor::Move(targets, {Items, {9, 1}, {9, 1}}, Direction::Right, false);
		Check(across == Place{Stash, {0, 7}, {0, 7}}, "a step from one grid into another keeps the height it was made at");
	}
	{
		// The shape of a skill tree: three columns with gaps, tabs beside them.
		std::vector<Target> targets;
		const int cells[][2] = {{0, 0}, {2, 0}, {1, 1}, {0, 2}, {2, 2}, {1, 3}, {2, 3}, {0, 4}, {1, 5}, {2, 5}};
		uintptr_t id = 1;
		for (const auto& cell : cells) targets.push_back({id++, {100 + cell[0] * 300, 100 + cell[1] * 250, 150, 150}, {}});
		for (int tab = 0; tab < 3; ++tab) targets.push_back({id++, {1100, 400 + tab * 300, 200, 250}, {}});
		targets.push_back({id++, {1150, 50, 80, 80}, {}});
		Check(AllReached(targets), "every skill, tab and button of a skill tree is reached from every other");
	}
	Passed();
	return 0;
}
