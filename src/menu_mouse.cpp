#include "menu_mouse.h"

#include "native_skill_navigation.h"

#include <windows.h>
#include <deque>

namespace D2RAccess::MenuMouse {
namespace {
// Policy, not game constants. The game reads the mouse once a frame.
constexpr uint64_t SettleMs = 60; // from a move of the pointer to a press on what it is on
constexpr uint64_t HoldMs = 40;   // from a press to its release

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
}

bool Place(const NativeUi::Rect& rect) {
	if (rect.width <= 0 || rect.height <= 0) return false;
	return NativeSkillNavigation::PlacePointer({rect.x + rect.width / 2, rect.y + rect.height / 2});
}

void Click(Button button) { Press(button, g_steps.empty() ? 0 : SettleMs); }

void ClickAt(const NativeUi::Rect& rect, Button button, const NativeUi::Rect& after) {
	g_steps.push_back({Act::Place, button, rect, g_steps.empty() ? 0 : SettleMs});
	Press(button, SettleMs);
	if (after.width > 0 && after.height > 0) g_steps.push_back({Act::Place, button, after, SettleMs});
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
	g_steps.pop_front();
	g_last = now;
	switch (step.act) {
	case Act::Place: (void)Place(step.rect); break;
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
