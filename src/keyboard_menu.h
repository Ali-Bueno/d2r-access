#pragma once

#include <D2RLPlugin/api.h>

// The arrow keys in a grid of items and in the skill tree. The game moves its
// cursor one cell for an arrow key; this finishes the move: on to the next
// item or empty cell past the rest of the item just left (grid_jump.h), or to
// the nearest skill that way (SkillGrid::Jump). The key itself is left to the
// game, so at the edge of a grid it still leaves the grid as it always did.
//
// With an item in hand the cursor goes cell by cell as before: where the item
// would be put down matters cell by cell. The pad's stick and D-pad are not
// touched. docs/keyboard-actions.md
namespace D2RAccess::KeyboardMenu {
// After KeyboardService::Tick and before the readers, so what is read is
// where the cursor ends up.
void Tick(const D2RL::PluginContext* context);
void Reset();
}
