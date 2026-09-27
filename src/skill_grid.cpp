#include "skill_grid.h"
#include <algorithm>
#include <cstdlib>
#include <tuple>

namespace D2RAccess::SkillGrid {
uintptr_t Move(std::span<const Node> nodes, uintptr_t selected, int dx, int dy) {
	if (nodes.empty() || std::abs(dx) + std::abs(dy) != 1) return 0;
	const auto from = std::find_if(nodes.begin(), nodes.end(), [selected](const Node& n) { return n.widget == selected; });
	if (from == nodes.end()) {
		return std::min_element(nodes.begin(), nodes.end(), [](const Node& a, const Node& b) {
			return std::tie(a.y, a.x) < std::tie(b.y, b.x);
		})->widget;
	}
	const Node* best = nullptr;
	std::tuple<int, int, int, int> bestScore;
	for (const auto& node : nodes) {
		const int forward = (node.x - from->x) * dx + (node.y - from->y) * dy;
		if (forward <= 0) continue;
		const int cross = std::abs((node.x - from->x) * dy - (node.y - from->y) * dx);
		const auto score = std::tuple(cross, forward, node.y, node.x);
		if (!best || score < bestScore) { best = &node; bestScore = score; }
	}
	return best ? best->widget : selected;
}
uintptr_t Jump(std::span<const Node> nodes, uintptr_t selected, int dx, int dy) {
	if (nodes.empty() || std::abs(dx) + std::abs(dy) != 1) return 0;
	const auto from = std::find_if(nodes.begin(), nodes.end(), [selected](const Node& n) { return n.widget == selected; });
	if (from == nodes.end()) return Move(nodes, selected, dx, dy);
	const Node* best = nullptr;
	std::tuple<int, int, int, int, int> bestScore;
	for (const auto& node : nodes) {
		const int forward = (node.x - from->x) * dx + (node.y - from->y) * dy;
		if (forward <= 0) continue;
		const int cross = std::abs((node.x - from->x) * dy - (node.y - from->y) * dx);
		// Up and down keep to the column while it has a skill that way.
		const int leaves = dy != 0 && cross != 0 ? 1 : 0;
		const auto score = std::tuple(leaves, forward, cross, node.y, node.x);
		if (!best || score < bestScore) { best = &node; bestScore = score; }
	}
	return best ? best->widget : selected;
}
}
