#pragma once
#include <cstdint>
#include <span>

namespace D2RAccess::SkillGrid {
struct Node { uintptr_t widget = 0; int x = 0, y = 0; };
// Preserve rows/columns where possible, skip gaps, and stop at the boundary.
// A missing selection enters at the first visible skill in on-screen order.
uintptr_t Move(std::span<const Node> nodes, uintptr_t selected, int dx, int dy);
}
