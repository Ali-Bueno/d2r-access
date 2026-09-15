#include "display_modes.h"

#include <windows.h>

#include <algorithm>

namespace D2RAccess::DisplayModes {

namespace {

// The game's dropdown starts at 1024x768: with this display it highlighted
// the current 1920x1080 at row 11, which is row 13 of the raw mode list
// (640x480 and 800x600 precede it). Verify against Settings.json
// "Screen Resolution (Windowed)" after applying a choice.
constexpr uint32_t MinimumWidth  = 1024;
constexpr uint32_t MinimumHeight = 768;

std::vector<Mode> Enumerate() {
	std::vector<Mode> modes;
	DEVMODEW          mode {};
	mode.dmSize = sizeof(mode);
	for (DWORD index = 0; EnumDisplaySettingsW(nullptr, index, &mode); ++index) {
		if (mode.dmPelsWidth >= MinimumWidth && mode.dmPelsHeight >= MinimumHeight) {
			modes.push_back({ mode.dmPelsWidth, mode.dmPelsHeight });
		}
	}
	std::sort(modes.begin(), modes.end(), [](const Mode& a, const Mode& b) { return a.width != b.width ? a.width < b.width : a.height < b.height; });
	modes.erase(std::unique(modes.begin(), modes.end(), [](const Mode& a, const Mode& b) { return a.width == b.width && a.height == b.height; }), modes.end());
	return modes;
}

}

const std::vector<Mode>& Modes() {
	static const std::vector<Mode> modes = Enumerate();
	return modes;
}

}
