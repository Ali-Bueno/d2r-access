#pragma once

#include <cstdint>
#include <optional>
#include <utility>

// An inventory cursor pushed against its grid's edge: the press the game's
// cursor does not follow (a tester, 2026-10-11: a "donk" at the inventory's
// sides). Pure logic over sampled focus and input, unit-tested without the game.
namespace D2RAccess::GridEdge {
// Policy: a fresh press that leaves the cell for SettleMs went nowhere (the game
// answers a press within a frame or two); a held push that has moved is only
// against the edge on the grid's last cell, still for longer than the key repeat.
inline constexpr uint64_t SettleMs = 150, HeldSettleMs = 600;

struct Sample {
	uintptr_t grid = 0;           // the focused grid widget, 0 off a grid
	int x = -1, y = -1;           // the cursor's cell
	int width = -1, height = -1;  // the grid's size in cells, -1 when unreadable
	int dx = 0, dy = 0;           // the pushed direction on one axis, 0 when released
	uint64_t now = 0;
};

class Detector {
public:
	// The pushed direction to sound, once per push held against the edge.
	std::optional<std::pair<int, int>> Update(const Sample& sample);
	void Reset() { *this = {}; }
private:
	Sample anchor_{};
	bool pushing_ = false, moved_ = false, sounded_ = false;
};
}
