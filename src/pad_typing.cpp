#include "pad_typing.h"

#include "async_log.h"
#include "input_direction.h"
#include "menu_mouse.h"
#include "native_skill_navigation.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <atomic>
#include <string>

namespace D2RAccess::PadTyping {
namespace {
// How long the game is given to start editing on its own after A on a box
// before the box is clicked for it.
constexpr uint64_t ClickAfterMs = 250;

std::atomic<bool> g_enabled{true};
uintptr_t g_box = 0;       // the box A was pressed on while nothing was being edited
uint64_t  g_pressedAt = 0;
bool      g_editing = false;
int       g_index = 0;     // the character the stick stands on

// A keystroke to the game as the keyboard's: a character, or Backspace.
bool SendKey(wchar_t character) {
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	if (process != GetCurrentProcessId()) return false;
	INPUT keys[2]{};
	keys[0].type = keys[1].type = INPUT_KEYBOARD;
	if (character) {
		keys[0].ki.wScan = keys[1].ki.wScan = static_cast<WORD>(character);
		keys[0].ki.dwFlags = KEYEVENTF_UNICODE;
		keys[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
	} else {
		keys[0].ki.wVk = keys[1].ki.wVk = VK_BACK;
		keys[1].ki.dwFlags = KEYEVENTF_KEYUP;
	}
	return SendInput(2, keys, sizeof(INPUT)) == 2;
}

std::string Word(char character) {
	return character == ' ' ? std::string(Strings::Current().spaceKey) : std::string(1, character);
}
}

int Step(int index, int delta) {
	const int count = static_cast<int>(Charset.size());
	return ((index + delta) % count + count) % count;
}

bool Enabled() { return g_enabled; }
void SetEnabled(bool on) { g_enabled = on; }

void Tick(const D2RL::PluginContext* context, NativeUi::Focus focus, std::optional<bool> editing, uint64_t now) {
	using InputDirection::FaceButton;
	using InputDirection::StickPush;
	if (!g_enabled) { Reset(); return; }
	const bool idle = editing && !*editing;
	const bool box = focus.widget && NativeUi::Classify(focus.widget).kind == UiText::Kind::TextInput;
	if (InputDirection::TakeFacePress(FaceButton::A)) {
		g_box = box && idle ? focus.widget : 0;
		g_pressedAt = now;
	}
	if (g_box && (!idle || focus.widget != g_box)) g_box = 0;
	if (g_box && now - g_pressedAt >= ClickAfterMs) {
		const uintptr_t target = g_box;
		g_box = 0;
		const auto rect = NativeSkillNavigation::FocusRect(target);
		if (rect) MenuMouse::ClickAt(*rect, MenuMouse::Button::Left);
		AsyncLog::InfoF(context, "[typing] A did not start editing %s: %s", NativeWidgets::ReadName(target).c_str(),
			rect ? "the box is clicked for it" : "and its rectangle is unknown");
	}
	if (!editing || !*editing) {
		g_editing = false;
		return;
	}
	if (!g_editing) {
		g_editing = true;
		g_index = 0;
		AsyncLog::InfoF(context, "[typing] editing %s: the right stick picks characters", NativeWidgets::ReadName(focus.widget).c_str());
	}
	if (InputDirection::PeekRightStickPush() == StickPush::None) return;
	const StickPush push = InputDirection::TakeRightStickPush("typing");
	switch (push) {
	case StickPush::Up:
	case StickPush::Down: {
		g_index = Step(g_index, push == StickPush::Up ? -1 : 1);
		ScreenReader::Speak(Word(Charset[static_cast<size_t>(g_index)]), true);
		break;
	}
	case StickPush::Right: {
		const char character = Charset[static_cast<size_t>(g_index)];
		const bool sent = SendKey(static_cast<wchar_t>(character));
		AsyncLog::InfoF(context, "[typing] '%c' %s", character, sent ? "sent" : "not sent: the game is not in front");
		break;
	}
	case StickPush::Left: {
		const bool sent = SendKey(0);
		AsyncLog::InfoF(context, "[typing] backspace %s", sent ? "sent" : "not sent: the game is not in front");
		break;
	}
	default: break;
	}
}

void Reset() {
	g_box = 0;
	g_pressedAt = 0;
	g_editing = false;
	g_index = 0;
}
}
