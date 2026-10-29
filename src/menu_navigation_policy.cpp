#include "menu_navigation_policy.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace D2RAccess::MenuNavigationPolicy {
namespace {
bool Valid(const Node& node) { return node.widget && node.width > 0 && node.height > 0; }

int64_t CenterX(const Node& node) { return 2LL * node.x + node.width; }

bool SameRow(const Node& a, const Node& b) {
	return std::min(int64_t(a.y) + a.height, int64_t(b.y) + b.height) > std::max(a.y, b.y);
}
}

uintptr_t Step(std::span<const Node> nodes, uintptr_t selected, int delta) {
	std::vector<uintptr_t> order;
	for (const auto& node : nodes) if (Valid(node)) order.push_back(node.widget);
	if (order.empty() || (delta != 1 && delta != -1)) return 0;
	const auto count = static_cast<std::ptrdiff_t>(order.size());
	const auto at = std::find(order.begin(), order.end(), selected) - order.begin();
	if (at == count) return delta > 0 ? order.front() : order.back();
	return order[static_cast<size_t>(((at + delta) % count + count) % count)];
}

uintptr_t Beside(std::span<const Node> nodes, uintptr_t selected, int dx) {
	if (dx != 1 && dx != -1) return 0;
	const auto current = std::find_if(nodes.begin(), nodes.end(), [selected](const Node& node) {
		return node.widget == selected && Valid(node);
	});
	if (current == nodes.end()) return Step(nodes, selected, dx);
	const Node* best = nullptr;
	int64_t bestDistance = 0;
	for (const auto& candidate : nodes) {
		if (!Valid(candidate) || candidate.widget == selected || !SameRow(*current, candidate)) continue;
		const int64_t distance = (CenterX(candidate) - CenterX(*current)) * dx;
		if (distance > 0 && (!best || distance < bestDistance)) { best = &candidate; bestDistance = distance; }
	}
	return best ? best->widget : selected;
}
}
