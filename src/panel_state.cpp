#include "panel_state.h"

#include <algorithm>
#include <string>
#include <vector>

namespace D2RAccess::PanelState {

namespace {

// Open panels, oldest first; the last element is the active one.
std::vector<std::string> g_open;

auto Find(std::string_view panel) {
	return std::find(g_open.begin(), g_open.end(), panel);
}

}

void Opened(std::string_view panel) {
	const auto it = Find(panel);
	if (it != g_open.end()) {
		g_open.erase(it);
	}
	g_open.emplace_back(panel);
}

bool Closed(std::string_view panel) {
	const auto it = Find(panel);
	if (it == g_open.end()) {
		return false;
	}
	const bool wasActive = (it + 1 == g_open.end());
	g_open.erase(it);
	return wasActive;
}

std::string_view Active() {
	return g_open.empty() ? std::string_view {} : std::string_view(g_open.back());
}

const std::vector<std::string>& OpenPanels() {
	return g_open;
}

void Reset() {
	g_open.clear();
}

}
