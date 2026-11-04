#include "menu_mouse.h"

#include "native_skill_navigation.h"

#include <windows.h>
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <optional>

namespace D2RAccess::MenuMouse {
namespace {
// Policy, not game constants. The game reads the mouse once a frame.
constexpr uint64_t SettleMs = 60; // from a move of the pointer to a press on what it is on
constexpr uint64_t HoldMs = 40;   // from a press to its release
constexpr int PointerSlack = 2;   // pixels a pointer may stand off a point and still be on it

enum class Act { Place, Down, Up, Wheel };
struct Step {
	Act act = Act::Place;
	Button button = Button::Left;
	NativeUi::Rect rect;
	uint64_t wait = 0; // after the step before it
	int key = 0;       // a release waits for this key to be let go
	int notches = 0;
};
std::deque<Step> g_steps;
uint64_t g_last = 0;   // when the last step was done
bool g_down[2]{};      // a press of ours still waits for its release

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

void Wheel(int notches) {
	INPUT input{};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_WHEEL;
	input.mi.mouseData = static_cast<DWORD>(notches * WHEEL_DELTA);
	(void)SendInput(1, &input, sizeof(input));
}

void Press(Button button, uint64_t settle, int heldBy) {
	g_steps.push_back({Act::Down, button, {}, settle});
	g_steps.push_back({Act::Up, button, {}, HoldMs, heldBy});
}
}

namespace {
// The point of the rectangle nearest its middle that a screen shows, in client pixels.
std::optional<NativeUi::Cell> Shown(const NativeUi::Rect& rect) {
	const NativeUi::Cell middle{rect.x + rect.width / 2, rect.y + rect.height / 2};
	const HWND window = GetForegroundWindow();
	POINT origin{0, 0}, screen{middle.x, middle.y};
	if (!ClientToScreen(window, &origin) || !ClientToScreen(window, &screen)) return std::nullopt;
	MONITORINFO monitor{};
	monitor.cbSize = sizeof(monitor);
	if (!GetMonitorInfoW(MonitorFromPoint(screen, MONITOR_DEFAULTTONEAREST), &monitor)) return std::nullopt;
	const auto& shown = monitor.rcMonitor;
	const NativeUi::Cell point{std::clamp<int>(screen.x, shown.left, shown.right - 1) - origin.x,
		std::clamp<int>(screen.y, shown.top, shown.bottom - 1) - origin.y};
	if (point.x < rect.x || point.y < rect.y || point.x >= rect.x + rect.width || point.y >= rect.y + rect.height) return std::nullopt;
	return point;
}
}

bool Within(NativeUi::Cell point) {
	const HWND window = GetForegroundWindow();
	RECT client{};
	if (!Foreground() || !GetClientRect(window, &client)) return false;
	return point.x >= client.left && point.y >= client.top && point.x < client.right && point.y < client.bottom;
}

bool At(NativeUi::Cell point) {
	POINT pointer{}, wanted{point.x, point.y};
	if (!GetCursorPos(&pointer) || !ClientToScreen(GetForegroundWindow(), &wanted)) return false;
	return std::abs(pointer.x - wanted.x) <= PointerSlack && std::abs(pointer.y - wanted.y) <= PointerSlack;
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
	const auto point = Shown(rect);
	return point && Within(*point) && NativeSkillNavigation::PlacePointer(*point);
}

void Click(Button button, int heldBy) { Press(button, g_steps.empty() ? 0 : SettleMs, heldBy); }

void ClickAt(const NativeUi::Rect& rect, Button button, const NativeUi::Rect& after, int heldBy) {
	g_steps.push_back({Act::Place, button, rect, g_steps.empty() ? 0 : SettleMs});
	Press(button, SettleMs, heldBy);
	if (after.width > 0 && after.height > 0) g_steps.push_back({Act::Place, button, after, SettleMs});
}

void WheelAt(const NativeUi::Rect& rect, int notches) {
	g_steps.push_back({Act::Place, Button::Left, rect, g_steps.empty() ? 0 : SettleMs});
	g_steps.push_back({Act::Wheel, Button::Left, {}, SettleMs, 0, notches});
	// Kept busy until the game has drawn the list where the wheel left it.
	g_steps.push_back({Act::Place, Button::Left, rect, SettleMs});
}

void PlaceAfter(const NativeUi::Rect& rect) {
	g_steps.push_back({Act::Place, Button::Left, rect, g_steps.empty() ? 0 : SettleMs});
}

bool Busy() { return !g_steps.empty(); }

void Tick(uint64_t now) {
	if (g_steps.empty()) return;
	if (!Foreground()) {
		// Another window has the mouse now: nothing more is clicked, and a
		// button of ours is not left held.
		Reset();
		return;
	}
	const Step step = g_steps.front();
	if (now - g_last < step.wait) return;
	if (step.act == Act::Up && step.key && (GetAsyncKeyState(step.key) & 0x8000)) return;
	g_steps.pop_front();
	g_last = now;
	switch (step.act) {
	case Act::Place:
		// Never a click on whatever the pointer happens to be over.
		if (!Place(step.rect)) Reset();
		break;
	case Act::Down:  Send(step.button, true); break;
	case Act::Up:    Send(step.button, false); break;
	case Act::Wheel: Wheel(step.notches); break;
	}
}

void Reset() {
	g_steps.clear();
	for (const auto button : {Button::Left, Button::Right})
		if (g_down[static_cast<size_t>(button)]) Send(button, false);
}
}
