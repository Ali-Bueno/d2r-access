#include "skill_grid.h"
#include "test_support.h"

#include <cstdint>
#include <set>
#include <vector>

using namespace D2RAccess;
using SkillGrid::Node;

namespace {
constexpr int Directions[][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

// A skill is named by where it stands: column first, then row, from one.
constexpr uintptr_t At(int column, int row) { return static_cast<uintptr_t>(column * 100 + row); }
Node Skill(int column, int row) { return {At(column, row), column, row}; }

std::set<uintptr_t> Reachable(const std::vector<Node>& nodes, uintptr_t start) {
	std::set<uintptr_t> met{start};
	std::vector<uintptr_t> queue{start};
	for (size_t head = 0; head < queue.size(); ++head)
		for (const auto& direction : Directions) {
			const auto next = SkillGrid::Jump(nodes, queue[head], direction[0], direction[1]);
			if (next && met.insert(next).second) queue.push_back(next);
		}
	return met;
}
// Every column a run of steps right stops in, from the leftmost skill.
std::set<int> Columns(const std::vector<Node>& nodes, uintptr_t start) {
	std::set<int> met;
	uintptr_t at = start;
	for (size_t guard = 0; guard <= nodes.size(); ++guard) {
		for (const auto& node : nodes)
			if (node.widget == at) met.insert(node.x);
		const auto next = SkillGrid::Jump(nodes, at, 1, 0);
		if (next == at) break;
		at = next;
	}
	return met;
}
// Every skill of one column a run of steps down meets, from its topmost skill,
// for as long as the run stays in the column.
std::set<uintptr_t> Column(const std::vector<Node>& nodes, uintptr_t start, int column) {
	std::set<uintptr_t> met;
	uintptr_t at = start;
	for (size_t guard = 0; guard <= nodes.size(); ++guard) {
		met.insert(at);
		const auto next = SkillGrid::Jump(nodes, at, 0, 1);
		if (next == at || static_cast<int>(next / 100) != column) break;
		at = next;
	}
	return met;
}
}

int main() {
	{
		// The user's own example: from column 1 row 1, the nearest skill to the
		// right is column 2 row 2; from there, column 3 row 2.
		const std::vector<Node> tree{Skill(1, 1), Skill(2, 2), Skill(3, 2), Skill(3, 1), Skill(1, 4)};
		Check(SkillGrid::Jump(tree, At(1, 1), 1, 0) == At(2, 2), "a step right takes the nearest skill that way, a row down if need be");
		Check(SkillGrid::Jump(tree, At(2, 2), 1, 0) == At(3, 2), "the next step right keeps to the row now stood on");
		Check(SkillGrid::Jump(tree, At(3, 2), 1, 0) == At(3, 2), "nothing lies past the last column");
		Check(SkillGrid::Jump(tree, At(3, 2), 0, -1) == At(3, 1), "a step up takes the skill above");
		Check(SkillGrid::Jump(tree, At(1, 1), 0, 1) == At(1, 4), "a step down keeps to the column while it has a skill below");
		Check(SkillGrid::Jump(tree, At(2, 2), 0, 1) == At(1, 4), "with none below in the column, a step down takes the nearest row that has one");
		Check(SkillGrid::Jump(tree, At(1, 4), 0, 1) == At(1, 4), "nothing lies below the last row");
		// The controller's step is unchanged: it keeps to the row and passes the nearer skill.
		Check(SkillGrid::Move(tree, At(1, 1), 1, 0) == At(3, 1), "the controller's step still keeps to its row");
	}
	{
		const std::vector<Node> tree{Skill(1, 1), Skill(2, 2)};
		Check(SkillGrid::Jump(tree, 0, 1, 0) == At(1, 1), "with nothing selected a step enters at the first skill");
		Check(SkillGrid::Jump(tree, At(1, 1), 1, 1) == 0, "a diagonal is not a step");
		Check(SkillGrid::Jump({}, At(1, 1), 1, 0) == 0, "an empty tree has nowhere to step");
	}
	{
		// No skill is ever passed by, on a thousand trees laid out at random in
		// the skill tree's three columns by six rows and in wider grids.
		uint32_t seed = 20260927;
		const auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
		bool reachable = true, columns = true, whole = true;
		for (int round = 0; round < 1000; ++round) {
			const int width = round % 2 ? 3 : 10, height = round % 2 ? 6 : 4;
			std::vector<Node> tree;
			std::set<int> allColumns;
			for (int y = 1; y <= height; ++y)
				for (int x = 1; x <= width; ++x)
					if (next() % 3 == 0) {
						tree.push_back(Skill(x, y));
						allColumns.insert(x);
					}
			if (tree.empty()) continue;
			uintptr_t leftmost = tree.front().widget;
			int left = tree.front().x;
			for (const auto& node : tree) {
				reachable &= Reachable(tree, node.widget).size() == tree.size();
				if (node.x < left) { left = node.x; leftmost = node.widget; }
			}
			columns &= Columns(tree, leftmost) == allColumns;
			for (const int column : allColumns) {
				std::set<uintptr_t> inColumn;
				uintptr_t topmost = 0;
				for (const auto& node : tree) // rows are laid out top first
					if (node.x == column) { inColumn.insert(node.widget); if (!topmost) topmost = node.widget; }
				whole &= Column(tree, topmost, column) == inColumn;
			}
		}
		Check(reachable, "every skill can be reached from every other");
		Check(columns, "a run of steps right stops in every column that holds a skill");
		Check(whole, "a run of steps down a column stops at every skill in it");
	}
	Passed();
	return 0;
}
