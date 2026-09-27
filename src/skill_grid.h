#pragma once
#include <cstdint>
#include <span>

namespace D2RAccess::SkillGrid {
struct Node { uintptr_t widget = 0; int x = 0, y = 0; };
// Preserve rows/columns where possible, skip gaps, and stop at the boundary.
// A missing selection enters at the first visible skill in on-screen order.
uintptr_t Move(std::span<const Node> nodes, uintptr_t selected, int dx, int dy);
// The keyboard's step. Left and right go to the nearest column that way
// holding any skill, and in it to the skill nearest the row stood on. Up and
// down keep to the column while it has a skill that way, and only then take
// the nearest row beyond. Every column is a stop and every skill of a column
// is a stop, so none is passed by: any skill is reached by going to its column
// and then along it. Stops at the boundary.
uintptr_t Jump(std::span<const Node> nodes, uintptr_t selected, int dx, int dy);
}
