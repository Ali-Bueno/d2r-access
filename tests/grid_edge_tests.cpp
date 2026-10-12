#include "grid_edge.h"
#include "test_support.h"

using namespace D2RAccess::GridEdge;

namespace {
constexpr uintptr_t Grid = 0x100;
Sample At(int x, int y, int dx, int dy, uint64_t now) { return {Grid, x, y, 10, 4, dx, dy, now}; }

void FreshPress() {
	Detector edge;
	Check(!edge.Update(At(9, 1, 1, 0, 0)), "a press is not judged on its first frame");
	Check(!edge.Update(At(9, 1, 1, 0, SettleMs - 1)), "nor before the game had time to move the cursor");
	const auto knock = edge.Update(At(9, 1, 1, 0, SettleMs));
	Check(knock && knock->first == 1 && knock->second == 0, "a press the cursor does not follow is against the edge, toward the push");
	Check(!edge.Update(At(9, 1, 1, 0, SettleMs * 4)), "held against it, it sounds once");
	Check(!edge.Update(At(9, 1, 0, 0, SettleMs * 5)), "releasing says nothing");
	Check(!edge.Update(At(9, 1, 1, 0, SettleMs * 6)) && edge.Update(At(9, 1, 1, 0, SettleMs * 7)), "and re-arms the next press");
}

void Moving() {
	Detector edge;
	edge.Update(At(3, 0, 1, 0, 0));
	Check(!edge.Update(At(4, 0, 1, 0, 50)), "a press the cursor follows is no edge");
	Check(!edge.Update(At(4, 0, 1, 0, 50 + HeldSettleMs)), "a held push waiting for the key repeat mid-row is no edge");
	edge.Update(At(9, 0, 1, 0, 1000));
	Check(!edge.Update(At(9, 0, 1, 0, 1000 + HeldSettleMs - 1)), "on the last column, the repeat may still be coming");
	Check(edge.Update(At(9, 0, 1, 0, 1000 + HeldSettleMs)).has_value(), "a held push stopped on the last column is against the edge");
	Detector up;
	up.Update(At(2, 2, 0, -1, 0));
	up.Update(At(2, 0, 0, -1, 40));
	const auto top = up.Update(At(2, 0, 0, -1, 40 + HeldSettleMs));
	Check(top && top->first == 0 && top->second == -1, "the top row is an edge too, centred");
}

void LeavingTheGrid() {
	Detector edge;
	edge.Update(At(9, 1, 1, 0, 0));
	Check(!edge.Update({0, -1, -1, -1, -1, 1, 0, SettleMs}), "a press that took focus off the grid (to equipment) is no edge");
	edge.Update(At(9, 1, 1, 0, 1000));
	Check(!edge.Update({Grid + 1, 0, 0, 10, 4, 1, 0, 1000 + SettleMs}), "nor one that landed on another grid");
	Check(!edge.Update(At(0, 0, -1, 0, 2000)) && !edge.Update(At(0, 0, 1, 0, 2000 + SettleMs - 1)),
		"turning the push starts over");
}
}

int main() {
	FreshPress(); Moving(); LeavingTheGrid();
	Passed();
}
