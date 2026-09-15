#pragma once

#include <cstdint>
#include <vector>

// The screen-resolution dropdown lists the primary display's modes, which the
// game builds at runtime (its layout declares no options). The same list is
// rebuilt here from Windows so a popup row index can be spoken as "W by H".
namespace D2RAccess::DisplayModes {

struct Mode {
	uint32_t width  = 0;
	uint32_t height = 0;
};

// Distinct width x height modes of the primary display at or above the game's
// minimum, ascending by width then height (the order the game's dropdown
// shows). Enumerated once.
const std::vector<Mode>& Modes();

}
