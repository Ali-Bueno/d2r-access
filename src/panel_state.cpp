#include "panel_state.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace D2RAccess::PanelState {

namespace {

// Open panels, oldest first; the last element is the active one.
std::vector<std::string> g_open;
std::string              g_announced;
// When the cinematic started, in steady-clock ticks; 0 when none plays. Read from the input
// callbacks as well as the ticks.
std::atomic<int64_t>     g_movieStart{0};
// A cinematic still "playing" this long after it started lost its end message: the longest act
// video is 427 s (d2intro). The keys and cues it holds must not stay held for good.
constexpr auto MovieLimit = std::chrono::minutes(8);

int64_t Now() {
	return std::chrono::steady_clock::now().time_since_epoch().count();
}

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

bool Hud(std::string_view panel) {
	return panel.starts_with("HUD") || panel == "PaladinAuraPanel";
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

void Announced(std::string_view title) {
	g_announced = title;
}

bool TakeAnnounced(std::string_view tab) {
	if (tab.empty() || tab != g_announced) {
		return false;
	}
	g_announced.clear();
	return true;
}

void MovieStarted() {
	g_movieStart = std::max<int64_t>(Now(), 1);
}

void MovieEnded() {
	g_movieStart = 0;
}

bool MoviePlaying() {
	const int64_t start = g_movieStart.load();
	return start != 0 && std::chrono::steady_clock::duration(Now() - start) < MovieLimit;
}

void Reset() {
	g_open.clear();
	g_announced.clear();
	g_movieStart = 0;
}

}
