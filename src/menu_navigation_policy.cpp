#include "menu_navigation_policy.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <tuple>

namespace D2RAccess::MenuNavigationPolicy {
namespace {
bool Valid(const Node& node) { return node.widget && node.width > 0 && node.height > 0; }

int64_t CenterX(const Node& node) { return 2LL * node.x + node.width; }
int64_t CenterY(const Node& node) { return 2LL * node.y + node.height; }

int64_t IntervalGap(int64_t firstStart, int64_t firstEnd, int64_t secondStart, int64_t secondEnd) {
	return std::max({0LL, secondStart - firstEnd, firstStart - secondEnd});
}
}

uintptr_t Move(std::span<const Node> nodes, uintptr_t selected, int dx, int dy) {
	if (std::abs(dx) + std::abs(dy) != 1) return 0;
	const auto valid = [](const Node& node) { return Valid(node); };
	const auto current = std::find_if(nodes.begin(), nodes.end(), [selected, &valid](const Node& node) {
		return node.widget == selected && valid(node);
	});
	if (current == nodes.end()) {
		const auto first = std::min_element(nodes.begin(), nodes.end(), [&valid](const Node& a, const Node& b) {
			if (valid(a) != valid(b)) return valid(a);
			return std::tie(a.y, a.x, a.widget) < std::tie(b.y, b.x, b.widget);
		});
		return first != nodes.end() && valid(*first) ? first->widget : 0;
	}

	const Node* best = nullptr;
	std::tuple<bool, int64_t, int64_t, int64_t, int64_t, int, int, uintptr_t> bestScore;
	for (const auto& candidate : nodes) {
		if (!valid(candidate) || candidate.widget == current->widget) continue;
		const int64_t forward = (CenterX(candidate) - CenterX(*current)) * dx
			+ (CenterY(candidate) - CenterY(*current)) * dy;
		if (forward <= 0) continue;

		const bool horizontal = dx != 0;
		const int64_t crossGap = horizontal
			? IntervalGap(current->y, int64_t(current->y) + current->height,
				candidate.y, int64_t(candidate.y) + candidate.height)
			: IntervalGap(current->x, int64_t(current->x) + current->width,
				candidate.x, int64_t(candidate.x) + candidate.width);
		const int64_t forwardGap = dx > 0 ? std::max(0LL, int64_t(candidate.x) - current->x - current->width)
			: dx < 0 ? std::max(0LL, int64_t(current->x) - candidate.x - candidate.width)
			: dy > 0 ? std::max(0LL, int64_t(candidate.y) - current->y - current->height)
			: std::max(0LL, int64_t(current->y) - candidate.y - candidate.height);
		const int64_t crossCenter = horizontal
			? std::abs(CenterY(candidate) - CenterY(*current))
			: std::abs(CenterX(candidate) - CenterX(*current));
		const auto score = std::tuple(crossGap != 0, forwardGap, forward, crossGap,
			crossCenter, candidate.y, candidate.x, candidate.widget);
		if (!best || score < bestScore) { best = &candidate; bestScore = score; }
	}
	return best ? best->widget : current->widget;
}
}
