#include "grid_edge.h"
#include "test_support.h"

using namespace D2RAccess::GridEdge;
using Kind = Outcome::Kind;

namespace {
constexpr uintptr_t Grid = 0x100;
Sample At(int x, int y, int dx, int dy, uint64_t now) { return {Grid, x, y, 10, 4, dx, dy, now}; }

void FreshPress() {
	Detector edge;
	Check(edge.Update(At(9, 1, 1, 0, 0)).kind == Kind::None, "a press is not judged on its first frame");
	Check(edge.Update(At(9, 1, 1, 0, SettleMs - 1)).kind == Kind::None, "nor before the game had time to move the cursor");
	const auto knock = edge.Update(At(9, 1, 1, 0, SettleMs));
	Check(knock.kind == Kind::Knock && knock.dx == 1 && knock.dy == 0 && knock.x == 9 && knock.y == 1,
		"a press the cursor does not follow is against the edge, toward the push");
	Check(edge.Update(At(9, 1, 1, 0, SettleMs * 4)).kind == Kind::None, "held against it, it sounds once");
	Check(edge.Update(At(9, 1, 0, 0, SettleMs * 5)).kind == Kind::None, "releasing says nothing");
	Check(edge.Update(At(9, 1, 1, 0, SettleMs * 6)).kind == Kind::None && edge.Update(At(9, 1, 1, 0, SettleMs * 7)).kind == Kind::Knock,
		"and re-arms the next press");
}

void Tap() {
	Detector edge;
	Check(edge.Update(At(0, 2, -1, 0, 0)).kind == Kind::None, "a tap on the left column");
	Check(edge.Update(At(0, 2, 0, 0, 40)).kind == Kind::None, "released before the game answered");
	Check(edge.Update(At(0, 2, 0, 0, SettleMs - 1)).kind == Kind::None, "still being judged");
	const auto knock = edge.Update(At(0, 2, 0, 0, SettleMs));
	Check(knock.kind == Kind::Knock && knock.dx == -1, "a tap the cursor does not follow knocks on the side pushed, after the release");
	Check(edge.Update(At(0, 2, 0, 0, SettleMs * 3)).kind == Kind::None, "once");

	Detector inside;
	inside.Update(At(4, 2, 1, 0, 0));
	inside.Update(At(4, 2, 0, 0, 40));
	Check(inside.Update(At(4, 2, 0, 0, SettleMs * 3)).kind == Kind::None, "a tap that went nowhere mid-row is no edge");

	Detector moved;
	moved.Update(At(8, 2, 1, 0, 0));
	moved.Update(At(8, 2, 0, 0, 40));
	Check(moved.Update(At(9, 2, 0, 0, 60)).kind == Kind::None && moved.Update(At(9, 2, 0, 0, SettleMs * 3)).kind == Kind::None,
		"a tap that moved the cursor onto the edge column is no edge");
}

void Moving() {
	Detector edge;
	edge.Update(At(3, 0, 1, 0, 0));
	Check(edge.Update(At(4, 0, 1, 0, 50)).kind == Kind::None, "a press the cursor follows is no edge");
	Check(edge.Update(At(4, 0, 1, 0, 50 + HeldSettleMs)).kind == Kind::None, "a held push waiting for the key repeat mid-row is no edge");
	edge.Update(At(9, 0, 1, 0, 1000));
	Check(edge.Update(At(9, 0, 1, 0, 1000 + HeldSettleMs - 1)).kind == Kind::None, "on the last column, the repeat may still be coming");
	Check(edge.Update(At(9, 0, 1, 0, 1000 + HeldSettleMs)).kind == Kind::Knock, "a held push stopped on the last column is against the edge");
	Detector up;
	up.Update(At(2, 2, 0, -1, 0));
	up.Update(At(2, 0, 0, -1, 40));
	const auto top = up.Update(At(2, 0, 0, -1, 40 + HeldSettleMs));
	Check(top.kind == Kind::Knock && top.dx == 0 && top.dy == -1, "the top row is an edge too, centred");
	Detector released;
	released.Update(At(8, 0, 1, 0, 0));
	released.Update(At(9, 0, 1, 0, 40));
	released.Update(At(9, 0, 0, 0, 80));
	Check(released.Update(At(9, 0, 0, 0, 40 + HeldSettleMs)).kind == Kind::None, "arriving on the edge and letting go is no knock");
}

void LeavingTheGrid() {
	Detector edge;
	edge.Update(At(9, 1, 1, 0, 0));
	const auto left = edge.Update({0, -1, -1, -1, -1, 1, 0, SettleMs});
	Check(left.kind == Kind::Left && left.x == 9 && left.y == 1, "a press that took focus off the grid is reported, with no knock");
	edge.Update(At(9, 1, 1, 0, 1000));
	Check(edge.Update({Grid + 1, 0, 0, 10, 4, 1, 0, 1000 + SettleMs}).kind == Kind::Left, "as is one that landed on another grid");
	Check(edge.Update(At(0, 0, -1, 0, 2000)).kind == Kind::None && edge.Update(At(0, 0, 1, 0, 2000 + SettleMs - 1)).kind == Kind::None,
		"turning the push starts over");
	Detector tapped;
	tapped.Update(At(9, 1, 1, 0, 0));
	tapped.Update(At(9, 1, 0, 0, 30));
	Check(tapped.Update({0, -1, -1, -1, -1, 0, 0, 60}).kind == Kind::Left, "a tap is followed past the release");
}

void Crossings() {
	constexpr uintptr_t Pack = 1, Vendor = 2;
	CrossingTracker tracker;
	Check(!tracker.Update(Pack, 1, 0, 1000).crossed, "staying on a panel is no crossing");
	const auto crossing = tracker.Update(Vendor, 0, 0, 1000 + PushWindowMs - 1);
	Check(crossing.crossed && crossing.dx == 1 && crossing.from == Pack && crossing.to == Vendor,
		"another panel's cell right after a push crosses, toward the push");
	Check(!tracker.Update(Pack, 0, 0, 1100).crossed, "going back inside the cool-down is not sounded");
	constexpr uint64_t firstCrossing = 1000 + PushWindowMs - 1;
	tracker.Update(Pack, -1, 0, firstCrossing + CrossingCooldownMs - 100);
	Check(tracker.Update(Vendor, -1, 0, firstCrossing + CrossingCooldownMs).crossed, "once the cool-down passed, the next crossing is");

	CrossingTracker blank;
	blank.Update(Pack, 1, 0, 0);
	blank.Update(0, 1, 0, PanelMemoryMs - 50);
	Check(blank.Update(Vendor, 0, 0, PanelMemoryMs).crossed, "a blank gap within the memory keeps the previous panel");
	CrossingTracker forgotten;
	forgotten.Update(Pack, 1, 0, 0);
	Check(!forgotten.Update(Vendor, 1, 0, PanelMemoryMs + 1).crossed, "a longer gap forgets it");
	CrossingTracker idle;
	idle.Update(Pack, 0, 0, 0);
	Check(!idle.Update(Vendor, 0, 0, 100).crossed, "a panel change with no push (the mouse, a tab) is no crossing");
	CrossingTracker stale;
	stale.Update(Pack, 1, 0, 0);
	stale.Update(Pack, 0, 0, PushWindowMs);
	stale.Update(Pack, 0, 0, PushWindowMs * 2);
	Check(!stale.Update(Vendor, 0, 0, PushWindowMs * 2 + 100).crossed, "a push long before is no crossing");
}
}

int main() {
	FreshPress(); Tap(); Moving(); LeavingTheGrid(); Crossings();
	Passed();
}
