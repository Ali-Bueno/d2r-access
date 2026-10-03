#pragma once

#include <string_view>

namespace D2RAccess::RadarUi {
enum class State { Playing, Inactive, Unavailable, Dialogue, Paused };
// UI thread only; use the shared open-panel state, not preloaded panel visibility.
State Inspect(bool foreground);
// A panel that is a menu by its name, whether or not the controller's menu
// switcher came up with it: in keyboard mode the game opens none (MEASURED
// 2026-09-27, docs/keyboard-play.md).
bool MenuPanel(std::string_view name);

// The one gate for the controls that act on the world -- guidance and the
// level scanner: the game has focus and no menu, dialogue
// or pause screen is up. In a menu the stick and its clicks belong to the
// menu. The user, 2026-09-21: "that will save us a lot of trouble for future
// context gates." UI thread only.
bool InPlay();
}
