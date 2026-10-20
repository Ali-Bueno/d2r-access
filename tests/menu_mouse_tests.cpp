#include <windows.h>
#include "native_skill_navigation.h"
#include "panel_state.h"
#include "test_support.h"
#include <vector>

// Compile the real queue against an isolated input backend. These fixtures
// never send mouse input or inspect/control a running game.
namespace {
bool inFront = true, canPlace = true;
std::vector<DWORD> sent;
std::vector<D2RAccess::NativeUi::Cell> placed;
HWND MockWindow() { return nullptr; }
DWORD MockProcess() { return 123; }
DWORD MockWindowProcess(HWND, DWORD* process) { *process = inFront ? 123 : 456; return 1; }
UINT MockSend(UINT count, INPUT* inputs, int) {
	for (UINT i = 0; i < count; ++i) sent.push_back(inputs[i].mi.dwFlags);
	return count;
}
}
namespace D2RAccess::NativeSkillNavigation {
bool PlacePointer(NativeUi::Cell point) noexcept {
	if (!canPlace) return false;
	placed.push_back(point);
	return true;
}
}
#define GetForegroundWindow MockWindow
#define GetCurrentProcessId MockProcess
#define GetWindowThreadProcessId MockWindowProcess
#define SendInput MockSend
#include "../src/menu_mouse.cpp"
#undef GetForegroundWindow
#undef GetCurrentProcessId
#undef GetWindowThreadProcessId
#undef SendInput

int main() {
	using namespace D2RAccess;
	using namespace MenuMouse;
	const NativeUi::Rect shop{20, 40, 10, 10}, back{60, 80, 10, 10};
	PanelState::Reset();
	PanelState::Opened("VendorPanel");
	auto revision = PanelState::Revision();
	Tick(1000, true, revision); // establish the current owner before input
	ClickAt(shop, Button::Left, back);
	Tick(1001, true, revision);
	Check(placed.size() == 1 && sent.empty(), "move precedes the click");
	Tick(1060, true, revision);
	Check(sent.empty(), "mouse settles for sixty milliseconds");
	Tick(1061, true, revision);
	Check(sent == std::vector<DWORD>{MOUSEEVENTF_LEFTDOWN}, "current menu receives one down");
	Tick(1100, true, revision);
	Check(sent.size() == 1, "button remains down for forty milliseconds");
	Tick(1101, true, revision);
	Check(sent.back() == MOUSEEVENTF_LEFTUP, "normal gesture releases");
	Tick(1161, true, revision);
	Check(placed.size() == 2 && !Busy(), "current-context return runs after release");

	sent.clear(); placed.clear();
	ClickAt(shop, Button::Left, back);
	Tick(2000, true, revision); // move only, then the menu disappears
	PanelState::Closed("VendorPanel");
	Tick(2100, false, PanelState::Revision());
	Tick(2200, true, PanelState::Revision());
	Check(sent.empty() && !Busy() && placed.size() == 1, "menu exit cancels pending down and return BEFORE dispatch");

	PanelState::Opened("VendorPanel"); revision = PanelState::Revision();
	Tick(3000, true, revision);
	ClickAt(shop, Button::Right, back);
	Tick(3001, true, revision); Tick(3061, true, revision);
	Check(sent == std::vector<DWORD>{MOUSEEVENTF_RIGHTDOWN}, "right gesture can own a held button");
	PanelState::Closed("VendorPanel"); PanelState::Opened("VendorPanel");
	Check(PanelState::Revision() != revision, "same panel reopened has a different lifetime");
	Tick(3062, true, PanelState::Revision());
	Check(sent == std::vector<DWORD>{MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP} && !Busy(), "owner change releases held button and retires return");
	Reset(); Check(sent.size() == 2, "cancellation does not duplicate a release");

	sent.clear(); placed.clear(); revision = PanelState::Revision();
	ClickAt(shop, Button::Left); canPlace = false;
	Tick(4000, true, revision); Tick(4100, true, revision);
	Check(sent.empty() && !Busy(), "failed pointer placement cannot click old HUD position");
	canPlace = true;
	ClickAt(shop, Button::Left);
	Tick(4110, false, revision); Tick(4120, true, revision);
	Check(sent.empty() && !Busy(), "play/visibility loss cancels even without a panel message");
	ClickAt(shop, Button::Left); inFront = false;
	Tick(4200, true, revision);
	Check(sent.empty() && !Busy(), "focus loss cancels before placing or pressing");
	inFront = true;
	for (int i = 0; i < 6; ++i) ClickAt(shop, Button::Left);
	Check(!Busy(), "six three-step gestures exceed bounded capacity and cancel the burst");
	const auto before = PanelState::Revision();
	PanelState::Closed("not-open");
	Check(PanelState::Revision() == before, "unknown close does not invalidate current input");
	PanelState::Reset();
	Check(PanelState::Revision() != before, "session reset retires panel lifetime");
	Passed();
}
