#pragma once

namespace D2RAccess::RadarUi {
enum class State { Playing, Inactive, Unavailable, Dialogue, Paused };
// UI thread only; use the shared open-panel state, not preloaded panel visibility.
State Inspect(bool foreground);
}
