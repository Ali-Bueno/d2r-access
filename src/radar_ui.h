#pragma once

namespace D2RAccess::RadarUi {
enum class State { Playing, Inactive, Unavailable, Dialogue, Paused };
// UI thread only; use the shared open-panel state, not preloaded panel visibility.
State Inspect(bool foreground);

// The one gate for the controls that act on the world -- guidance, the
// pathfinder, the short-range scan: the game has focus and no menu, dialogue
// or pause screen is up. In a menu the stick and its clicks belong to the
// menu. The user, 2026-09-21: "that will save us a lot of trouble for future
// context gates." UI thread only.
bool InPlay();
}
