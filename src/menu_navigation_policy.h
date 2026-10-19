#pragma once

#include <cstdint>
#include <span>

namespace D2RAccess::MenuNavigationPolicy {
struct Node { uintptr_t widget = 0; int x = 0, y = 0, width = 0, height = 0; };

// Nodes are visible, enabled controls with absolute focus rectangles.
uintptr_t Move(std::span<const Node> nodes, uintptr_t selected, int dx, int dy);
// The next (delta 1) or previous (-1) control in the form's own order, the order `nodes`
// come in, wrapping at the ends; from no control, the first or the last.
uintptr_t Step(std::span<const Node> nodes, uintptr_t selected, int delta);
}
