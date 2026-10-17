#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <string>
#include <string_view>

// What the waypoint panel shows of a row: gold (can be travelled to), blue (the
// player stands there) or grey (never activated). The game keeps one table for
// the rows of the act tab the panel shows, rebuilt on every tab change.
// docs/waypoints.md
namespace D2RAccess::NativeWaypoints {
enum class State { Unknown, Open, Here, Undiscovered };
struct Row {
	State state = State::Unknown;
	// What it was decided from, for the log.
	int32_t tab = -1, count = 0, level = 0, player = 0;
	uint8_t open = 0;
};
bool Initialize(const D2RL::PluginContext* context);
// The row named `name` ("Act1 Button3") of the waypoint panel `panel`. Unknown
// when the image is not the one read, or the table is not that act's. UI thread.
Row Read(uintptr_t panel, std::string_view name) noexcept;
const char* Name(State state);
// NativeUi::FocusState for the waypoint panels: "undiscovered", "you are here" or nothing.
std::string FocusWords(std::string_view panelName, uintptr_t panel, std::string_view control);
}
