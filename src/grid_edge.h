#pragma once

#include <cstdint>

// An inventory cursor pushed against its grid's edge: the press the game's
// cursor does not follow (a tester, 2026-10-11: a "donk" at the inventory's
// sides). Pure logic over sampled focus and input, unit-tested without the game.
namespace D2RAccess::GridEdge {
// Policy: a fresh press on the edge cell that leaves the cell for SettleMs went
// nowhere (the game answers within a frame or two), even if the button is already
// up: a tap is shorter than that. A held push that has moved is only against the
// edge on the grid's last cell, still for longer than the key repeat.
inline constexpr uint64_t SettleMs = 100, HeldSettleMs = 600;

struct Sample {
	uintptr_t grid = 0;           // the focused grid widget, 0 off a grid
	int x = -1, y = -1;           // the cursor's cell
	int width = -1, height = -1;  // the grid's size in cells, -1 when unreadable
	int dx = 0, dy = 0;           // the pushed direction on one axis, 0 when released
	uint64_t now = 0;
};

// What became of a press on an edge cell: the cursor stayed (Knock, toward the
// push) or focus left the grid (Left); x, y is the cell it was pressed on.
struct Outcome {
	enum class Kind { None, Knock, Left } kind = Kind::None;
	int dx = 0, dy = 0, x = -1, y = -1;
};

// Policy: a push or tap within PushWindowMs before a cell of another panel's grid
// is focused took the cursor across; the previous panel is remembered across a
// blank-focus gap of PanelMemoryMs, and crossings are CrossingCooldownMs apart at
// least. Values are tuning for the game's one-frame focus switch, from tester logs.
inline constexpr uint64_t PushWindowMs = 300, PanelMemoryMs = 400, CrossingCooldownMs = 500;

// The cursor moved from one panel's grid into another's (backpack to vendor or stash).
struct Crossing {
	bool crossed = false;
	int dx = 0, dy = 0;           // the direction last pushed
	uintptr_t from = 0, to = 0;   // the panels
};

class CrossingTracker {
public:
	// panel: the panel of the focused grid cell, 0 when focus is not on a grid cell.
	Crossing Update(uintptr_t panel, int dx, int dy, uint64_t now);
	void Reset() { *this = {}; }
private:
	uintptr_t panel_ = 0;
	uint64_t panelAt_ = 0, pushAt_ = 0, crossedAt_ = 0;
	int dx_ = 0, dy_ = 0;
	bool pushed_ = false, crossedBefore_ = false;
};

class Detector {
public:
	// Once per press against the edge, or per held push that stopped there.
	Outcome Update(const Sample& sample);
	void Reset() { *this = {}; }
private:
	Outcome Judge(const Sample& sample);
	Sample anchor_{};
	bool pushing_ = false, pending_ = false, moved_ = false, sounded_ = false;
};
}
