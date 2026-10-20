#include "menu_mouse.h"

#include "native_skill_navigation.h"

#include <windows.h>
#include <deque>

namespace D2RAccess::MenuMouse {
namespace {
// Policy, not game constants. The game reads the mouse once a frame.
constexpr uint64_t SettleMs = 60; // from a move of the pointer to a press on what it is on
constexpr uint64_t HoldMs = 40;   // from a press to its release
constexpr size_t MaxSteps = 16; // bounded burst; overload retires the gesture

enum class Act { Place, Down, Up };
struct Step {
	Act act = Act::Place;
	Button button = Button::Left;
	NativeUi::Rect rect;
	uint64_t wait = 0; // after the step before it
};
std::deque<Step> g_steps;
uint64_t g_last = 0;   // when the last step was done
bool g_down[2]{};      // a press of ours still waits for its release
uint64_t g_revision = 0;

bool Foreground() {
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return process == GetCurrentProcessId();
}

void Send(Button button, bool down) {
	INPUT input{};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = button == Button::Left ? (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP)
		: (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
	(void)SendInput(1, &input, sizeof(input));
	g_down[static_cast<size_t>(button)] = down;
}

void Press(Button button, uint64_t settle) {
	g_steps.push_back({Act::Down, button, {}, settle});
	g_steps.push_back({Act::Up, button, {}, HoldMs});
}
bool Room(size_t count) {
	if (g_steps.size() + count <= MaxSteps) return true;
	Reset();
	return false;
}
}

void Nudge() {
	POINT at{};
	if (!GetCursorPos(&at)) return;
	const int left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	if (width <= 1 || height <= 1) return;
	// Absolute places: a move of one count relative can come to no pixel at all
	// once the pointer's acceleration has had its say.
	const POINT points[2] = {{at.x + 2, at.y}, at};
	INPUT inputs[2]{};
	for (int i = 0; i < 2; ++i) {
		inputs[i].type = INPUT_MOUSE;
		inputs[i].mi.dx = static_cast<LONG>((static_cast<long long>(points[i].x - left) * 65535) / (width - 1));
		inputs[i].mi.dy = static_cast<LONG>((static_cast<long long>(points[i].y - top) * 65535) / (height - 1));
		inputs[i].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
	}
	(void)SendInput(2, inputs, sizeof(INPUT));
}

bool Place(const NativeUi::Rect& rect) {
	if (rect.width <= 0 || rect.height <= 0) return false;
	return NativeSkillNavigation::PlacePointer({rect.x + rect.width / 2, rect.y + rect.height / 2});
}

void Click(Button button) {
	if (Room(2)) Press(button, g_steps.empty() ? 0 : SettleMs);
}

void ClickAt(const NativeUi::Rect& rect, Button button, const NativeUi::Rect& after) {
	if (!Room(after.width > 0 && after.height > 0 ? 4 : 3)) return;
	g_steps.push_back({Act::Place, button, rect, g_steps.empty() ? 0 : SettleMs});
	Press(button, SettleMs);
	if (after.width > 0 && after.height > 0) g_steps.push_back({Act::Place, button, after, SettleMs});
}

void PlaceAfter(const NativeUi::Rect& rect) {
	if (!Room(1)) return;
	g_steps.push_back({Act::Place, Button::Left, rect, g_steps.empty() ? 0 : SettleMs});
}

bool Busy() { return !g_steps.empty(); }

void Tick(uint64_t now, bool menuActive, uint64_t panelRevision) {
	const bool changed = panelRevision != g_revision;
	g_revision = panelRevision;
	if (!menuActive || changed) { Reset(); return; }
	if (g_steps.empty()) return;
	if (!Foreground()) {
		// Another window has the mouse now: nothing more is clicked, and a
		// button of ours is not left held.
		Reset();
		return;
	}
	const Step step = g_steps.front();
	if (now - g_last < step.wait) return;
	g_steps.pop_front();
	g_last = now;
	switch (step.act) {
	case Act::Place:
		// Failure must not turn the following press into a click at the old
		// pointer (which can now be a Character/HUD button after a shop).
		if (!Place(step.rect)) Reset();
		break;
	case Act::Down:  Send(step.button, true); break;
	case Act::Up:    Send(step.button, false); break;
	}
}

void Reset() {
	g_steps.clear();
	for (const auto button : {Button::Left, Button::Right})
		if (g_down[static_cast<size_t>(button)]) Send(button, false);
}
}
