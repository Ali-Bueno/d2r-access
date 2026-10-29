#pragma once

#include <cstdint>
#include <span>

namespace D2RAccess::MenuNavigationPolicy {
struct Node { uintptr_t widget = 0; int x = 0, y = 0, width = 0, height = 0; };

// Nodes are visible, enabled controls with absolute focus rectangles.
// The nearest control left (dx -1) or right (1) of the selected one on its own row (their
// rectangles overlap vertically); the selected one when there is none. From no control, as Step.
uintptr_t Beside(std::span<const Node> nodes, uintptr_t selected, int dx);
// The next (delta 1) or previous (-1) control in the form's own order, the order `nodes`
// come in, wrapping at the ends; from no control, the first or the last.
uintptr_t Step(std::span<const Node> nodes, uintptr_t selected, int delta);
}
