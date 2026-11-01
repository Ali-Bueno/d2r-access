#include "menu_cursor.h"
#include "generated/skill_tree.h"
#include "test_support.h"

#include <algorithm>
#include <cstdint>
#include <utility>
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

bool AllReached(const std::vector<Target>& targets, const std::vector<MenuCursor::Link>& links = {}) {
	const auto all = MenuCursor::Everything(targets);
	for (const auto& from : all) {
		const auto reached = MenuCursor::Reachable(targets, from, links);
		if (reached.size() != all.size()) return false;
		for (const auto& place : all)
			if (std::find(reached.begin(), reached.end(), place) == reached.end()) return false;
	}
	return true;
}

// One page of a class's skill tree as the panel draws it: the rows and columns
// of the mouse layout (SkillsTreePanelHD.json, skillRow and skillColumn), a
// skill `size` across, the three tabs over them and the close button. A
// skill's widget is its skill id.
constexpr int SkillRows[] = {296, 478, 654, 834, 1014, 1190}, SkillColumns[] = {253, 515, 775};
constexpr uintptr_t FirstTab = 1000, CloseTree = 2000, Skills = 1;
std::vector<Target> SkillPage(int playerClass, int page, int size) {
	std::vector<Target> targets;
	for (const auto& skill : Generated::SkillTree)
		if (skill.playerClass == playerClass && skill.page == page)
			targets.push_back({skill.skill, {SkillColumns[skill.column - 1], SkillRows[skill.row - 1], size, size}, {}, Skills});
	for (int tab = 0; tab < 3; ++tab) targets.push_back({FirstTab + tab, {130 + tab * 298, 160, 294, 99}, {}});
	targets.push_back({CloseTree, {1075, 9, 60, 60}, {}});
	return targets;
}
uintptr_t Step(const std::vector<Target>& targets, uintptr_t from, Direction direction) {
	return MenuCursor::Move(targets, {from, {}, {}}, direction, false).widget;
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
		bool reached = true, unmended = true;
		for (int round = 0; round < 60; ++round) {
			auto targets = Inventory(Random(seed, 10, 4));
			targets.push_back({Stash, {1200, 200, 1000, 1000}, Random(seed, 10, 10)});
			reached &= AllReached(targets);
			unmended &= MenuCursor::Mend(targets).empty();
		}
		Check(reached, "every control, item and empty cell is reached from every other, in sixty menus filled at random");
		Check(unmended, "and none of them is given a step it does not need");
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
	{
		// The Paladin's combat skills (class 3, page 1), where the first row has a
		// skill in columns 1 and 3 and the second row one in column 2.
		enum : uintptr_t { Sacrifice = 96, Smite = 97, HolyBolt = 101, Zeal = 106, Charge = 107, Vengeance = 111, BlessedHammer = 112,
			Conversion = 116, HolyShield = 117, FistOfTheHeavens = 121 };
		const auto targets = SkillPage(3, 1, 120);
		Check(targets.size() == 14, "a page of a skill tree has ten skills");
		Check(Step(targets, Sacrifice, Direction::Right) == HolyBolt,
			"a step to the right from row 1, column 1 lands on row 2, column 2 when row 1 has no skill in column 2");
		// Row 3 is drawn a little nearer to row 2 than row 1 is (176 against 182).
		Check(Step(targets, HolyBolt, Direction::Right) == Charge, "and the next lands in column 3, on the nearer of the two beside it");
		Check(Step(targets, Smite, Direction::Left) == HolyBolt && Step(targets, HolyBolt, Direction::Left) == Zeal,
			"and a step to the left lands in the column before in the same way");
		Check(Step(targets, Charge, Direction::Up) == Smite && Step(targets, Zeal, Direction::Up) == Sacrifice,
			"a step up keeps to the column");
		Check(Step(targets, Sacrifice, Direction::Down) == Zeal && Step(targets, Smite, Direction::Down) == Charge,
			"a step down keeps to the column that has a skill further down");
		Check(Step(targets, HolyBolt, Direction::Down) == BlessedHammer, "and passes a row that has none in that column");
		Check(Step(targets, BlessedHammer, Direction::Down) == FistOfTheHeavens && Step(targets, FistOfTheHeavens, Direction::Down) == FistOfTheHeavens,
			"down to the last row, past which nothing lies");
		Check(Step(targets, HolyShield, Direction::Left) == BlessedHammer || Step(targets, HolyShield, Direction::Left) == FistOfTheHeavens,
			"a step to the left from a row with no skill in column 2 lands in column 2");
	}
	{
		// Every page of every class, at each size a skill may be drawn at.
		bool reached = true, unmended = true;
		int pages = 0;
		for (const int size : {60, 96, 120, 160}) {
			for (int playerClass = 0; playerClass < Generated::SkillTreeClasses; ++playerClass) {
				for (int page = 1; page <= Generated::SkillTreePages; ++page) {
					const auto targets = SkillPage(playerClass, page, size);
					++pages;
					unmended &= MenuCursor::Mend(targets).empty();
					reached &= AllReached(targets);
				}
			}
		}
		Check(pages == 96, "eight classes have three pages each");
		Check(reached, "every skill of every page of every class is reached from every other, with the tabs and the close button");
		Check(unmended, "and by the arrows as they are: no page needs a step made for it");
	}
	{
		// Controls strewn at random: what the arrows alone miss is put within
		// their reach, and nothing they reached is lost by it.
		uint32_t seed = 927;
		const auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return static_cast<int>((seed >> 8) % 2000); };
		int missing = 0, mended = 0;
		for (int round = 0; round < 400; ++round) {
			std::vector<Target> targets;
			const int count = 3 + next() % 12;
			for (int i = 0; i < count; ++i)
				targets.push_back({static_cast<uintptr_t>(i + 1), {next(), next(), 20 + next() % 200, 20 + next() % 100}, {},
					static_cast<uintptr_t>(next() % 3)});
			const auto all = MenuCursor::Everything(targets);
			const auto first = MenuCursor::First(targets);
			if (MenuCursor::Reachable(targets, first).size() == all.size()) continue;
			++missing;
			if (AllReached(targets, MenuCursor::Mend(targets))) ++mended;
		}
		Check(mended == missing, "in every menu laid out at random every place is reached from every other once it is mended");
	}
	{
		// A button in the middle of four skills: every step from a skill that
		// could land on it lands on a skill, so the arrows alone pass it by.
		enum : uintptr_t { TopLeft = 1, TopRight, LowLeft, LowRight, Button };
		const std::vector<Target> targets{{TopLeft, {0, 0, 50, 50}, {}, Skills}, {TopRight, {200, 0, 50, 50}, {}, Skills},
			{LowLeft, {0, 200, 50, 50}, {}, Skills}, {LowRight, {200, 200, 50, 50}, {}, Skills}, {Button, {100, 100, 50, 50}, {}}};
		Check(MenuCursor::Reachable(targets, MenuCursor::First(targets)).size() == 4, "a place the arrows alone do not reach");
		const auto links = MenuCursor::Mend(targets);
		Check(links.size() == 3, "is put within their reach by three steps made for it");
		Check(AllReached(targets, links), "and then every place is reached from every other");
		Check(MenuCursor::Move(targets, {TopLeft, {}, {}}, Direction::Right, false, links).widget == Button,
			"the step of the place nearest to it lands on it");
		Check(MenuCursor::Move(targets, {Button, {}, {}}, Direction::Right, false, links).widget == TopRight,
			"the same step again lands where that step landed before");
		Check(MenuCursor::Move(targets, {Button, {}, {}}, Direction::Left, false, links).widget == TopLeft, "and the step back returns");
	}
	{
		// The Warcries page as the live log drew it (2026-11-01): down from Howl
		// landed on the footer, put there by Mend because the tabs' down step
		// passed over the skills to the footer, a thing of no group like a tab.
		enum : uintptr_t { Cancel = 1, Tab0, Tab1, Tab2, Footer, Tree = 100 };
		std::vector<Target> targets{{Cancel, {2347, 144, 55, 56}, {}}, {Tab0, {1717, 244, 196, 66}, {}},
			{Tab1, {1916, 244, 196, 66}, {}}, {Tab2, {2113, 244, 196, 66}, {}}, {Footer, {1691, 1063, 648, 40}, {}}};
		const int column[] = {1799, 1974, 2147};
		const std::pair<int, int> cells[] = {{0, 335}, {2, 335}, {0, 456}, {1, 456}, {2, 574}, {0, 694}, {1, 814}, {2, 814}, {0, 931}, {1, 931}};
		uintptr_t skill = 10;
		for (const auto& [x, y] : cells) targets.push_back({skill++, {column[x], y, 86, 86}, {}, Tree});
		constexpr uintptr_t Howl = 10, FindPotion = 12, GrimWard = 18;
		Check(MenuCursor::Move(targets, {Tab0, {}, {}}, Direction::Down, false).widget == Howl, "down from a tab lands on the skill under it");
		const auto links = MenuCursor::Mend(targets);
		Check(MenuCursor::Move(targets, {Howl, {}, {}}, Direction::Down, false, links).widget == FindPotion,
			"down from the first skill lands on the skill below it, not on the footer");
		Check(MenuCursor::Move(targets, {GrimWard, {}, {}}, Direction::Down, false, links).widget == Footer, "the footer is below the last row");
		Check(AllReached(targets, links), "and every place of the page is reached from every other");
	}
	{
		enum : uintptr_t { Top = 1, Low, Hidden };
		const std::vector<Target> targets{{Top, {0, 0, 100, 100}, {}}, {Low, {0, 200, 100, 100}, {}}, {Hidden, {0, 0, 0, 0}, {}}};
		Check(MenuCursor::Mend(targets).empty(), "a thing that is not drawn is no place, and is not missed");
	}
	Passed();
	return 0;
}
