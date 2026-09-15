#include "display_modes.h"

#include <windows.h>

#include <algorithm>

namespace D2RAccess::DisplayModes {

namespace {

std::vector<Mode> Enumerate() {
	std::vector<Mode> modes;
	DEVMODEW          mode {};
	mode.dmSize = sizeof(mode);
	for (DWORD index = 0; EnumDisplaySettingsW(nullptr, index, &mode); ++index) {
		modes.push_back({ mode.dmPelsWidth, mode.dmPelsHeight });
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
