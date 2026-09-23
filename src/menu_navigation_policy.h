#pragma once

#include <cstdint>
#include <span>

namespace D2RAccess::MenuNavigationPolicy {
struct Node { uintptr_t widget = 0; int x = 0, y = 0, width = 0, height = 0; };

// Nodes are visible, enabled controls with absolute focus rectangles.
uintptr_t Move(std::span<const Node> nodes, uintptr_t selected, int dx, int dy);
}
