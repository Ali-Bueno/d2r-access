#include "pad_typing.h"

#include "async_log.h"
#include "input_direction.h"
#include "native_skill_navigation.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <string>

namespace D2RAccess::PadTyping {
namespace {
// How long the game is given to start editing on its own after A on a box, before
// the mod starts it the way the game's A does. Never with a mouse click: a real mouse
// event switches every panel to its mouse layout, closing Direct Join (the user, 2026-10-17).
constexpr uint64_t EditWaitMs = 250;

std::atomic<bool> g_enabled{true}, g_qwerty{true};
uintptr_t g_box = 0;       // the box A was pressed on while nothing was being edited
uint64_t  g_pressedAt = 0;
bool      g_editing = false;
int       g_index = 0;     // the character the stick stands on
Key       g_key;           // the same on the QWERTY keyboard
bool      g_capsLock = false;

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
	const auto& strings = Strings::Current();
	if (character == CapsLockKey) return strings.capsLockKey;
	if (character == BackspaceKey) return strings.backspaceKey;
	return character == ' ' ? std::string(strings.spaceKey) : std::string(1, character);
}

int Wrap(int index, int delta, int count) { return ((index + delta) % count + count) % count; }
}

int Step(int index, int delta) { return Wrap(index, delta, static_cast<int>(Charset.size())); }

Key Move(Key key, int dx, int dy) {
	key.row = Wrap(key.row, dy, static_cast<int>(QwertyRows.size()));
	const int count = static_cast<int>(QwertyRows[static_cast<size_t>(key.row)].size());
	key.column = Wrap(std::min(key.column, count - 1), dx, count);
	return key;
}

char KeyAt(Key key, bool capsLock) {
	const char character = QwertyRows[static_cast<size_t>(key.row)][static_cast<size_t>(key.column)];
	return capsLock && character >= 'a' && character <= 'z' ? static_cast<char>(character - 'a' + 'A') : character;
}

bool Enabled() { return g_enabled; }
void SetEnabled(bool on) { g_enabled = on; }
bool Qwerty() { return g_qwerty; }
void SetQwerty(bool on) { g_qwerty = on; }

void Tick(const D2RL::PluginContext* context, NativeUi::Focus focus, std::optional<bool> editing, uint64_t now) {
	using InputDirection::FaceButton;
	using InputDirection::StickPush;
	if (!g_enabled) { Reset(); return; }
	const bool idle = editing && !*editing;
	const bool box = focus.widget && NativeUi::Classify(focus.widget).kind == UiText::Kind::TextInput;
	const bool pressedA = InputDirection::TakeFacePress(FaceButton::A);
	if (pressedA) {
		g_box = box && idle ? focus.widget : 0;
		g_pressedAt = now;
	}
	if (g_box && (!idle || focus.widget != g_box)) g_box = 0;
	if (g_box && now - g_pressedAt >= EditWaitMs) {
		const char* failed = NativeSkillNavigation::BeginEdit(g_box);
		AsyncLog::InfoF(context, "[typing] A did not start editing %s: %s", NativeWidgets::ReadName(g_box).c_str(),
			failed ? failed : "begun by the mod");
		g_box = 0;
	}
	if (!editing || !*editing) {
		g_editing = false;
		return;
	}
	// The game's text entry panel says nothing of itself (a tester, 2026-10-17: A seemed to do nothing).
	if (!g_editing) {
		g_editing = true;
		g_index = 0;
		g_key = {};
		g_capsLock = false;
		AsyncLog::InfoF(context, "[typing] editing %s: the right stick picks characters", NativeWidgets::ReadName(focus.widget).c_str());
		ScreenReader::Speak(g_qwerty ? Strings::Current().typingStartedQwerty : Strings::Current().typingStarted, true);
		return; // the A that began the edit is not a character
	}
	const auto send = [&](char character) {
		const bool sent = SendKey(character == BackspaceKey ? 0 : static_cast<wchar_t>(character));
		const char* outcome = sent ? "sent" : "not sent: the game is not in front";
		if (character == BackspaceKey) AsyncLog::InfoF(context, "[typing] backspace %s", outcome);
		else AsyncLog::InfoF(context, "[typing] '%c' %s", character, outcome);
	};
	const auto type = [&] {
		if (!g_qwerty) return send(Charset[static_cast<size_t>(g_index)]);
		const char character = KeyAt(g_key, g_capsLock);
		if (character != CapsLockKey) return send(character);
		g_capsLock = !g_capsLock;
		ScreenReader::Speak(g_capsLock ? Strings::Current().upperCase : Strings::Current().lowerCase, true);
	};
	if (pressedA) type();
	if (InputDirection::PeekRightStickPush() == StickPush::None) return;
	const StickPush push = InputDirection::TakeRightStickPush("typing");
	if (g_qwerty) {
		const int dx = push == StickPush::Right ? 1 : push == StickPush::Left ? -1 : 0;
		const int dy = push == StickPush::Down ? 1 : push == StickPush::Up ? -1 : 0;
		g_key = Move(g_key, dx, dy);
		ScreenReader::Speak(Word(KeyAt(g_key, g_capsLock)), true);
		return;
	}
	switch (push) {
	case StickPush::Up:
	case StickPush::Down: {
		// Up goes forward through the letters, as most games' character pickers do (a tester, 2026-10-17).
		g_index = Step(g_index, push == StickPush::Up ? 1 : -1);
		ScreenReader::Speak(Word(Charset[static_cast<size_t>(g_index)]), true);
		break;
	}
	case StickPush::Right: type(); break;
	case StickPush::Left: send(BackspaceKey); break;
	default: break;
	}
}

void Reset() {
	// An A pressed before the form showed (the one that opened it) is not a press on a box.
	(void)InputDirection::TakeFacePress(InputDirection::FaceButton::A);
	g_box = 0;
	g_pressedAt = 0;
	g_editing = false;
	g_index = 0;
	g_key = {};
	g_capsLock = false;
}
}
