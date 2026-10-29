#include "pad_typing.h"
#include "async_log.h"
#include "input_direction.h"
#include "native_skill_navigation.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "test_support.h"

#include <string>
#include <utility>
#include <vector>

namespace {
using namespace D2RAccess;
using InputDirection::FaceButton;
using InputDirection::StickPush;
bool pressedA = false;
StickPush push = StickPush::None;
std::vector<std::string> spoken;

void Tick(bool a = false, StickPush stick = StickPush::None) {
	pressedA = a;
	push = stick;
	PadTyping::Tick(nullptr, {1, 10}, true, 0);
}
}

namespace D2RAccess::InputDirection {
bool TakeFacePress(FaceButton button) {
	const bool pressed = button == FaceButton::A && pressedA;
	pressedA = false;
	return pressed;
}
StickPush PeekRightStickPush() { return push; }
StickPush TakeRightStickPush(const char*) { return std::exchange(push, StickPush::None); }
}
namespace D2RAccess::NativeSkillNavigation { const char* BeginEdit(uintptr_t) { return nullptr; } }
namespace D2RAccess::NativeWidgets { std::string ReadName(uintptr_t) { return "HostGameNameInput"; } }
namespace D2RAccess::NativeUi { Type Classify(uintptr_t) { return {"", UiText::Kind::TextInput, UiText::Surface::Other}; } }
namespace D2RAccess::ScreenReader { void Speak(std::string_view text, bool) { spoken.emplace_back(text); } }
namespace D2RAccess::AsyncLog { void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {} }

int main() {
	using namespace D2RAccess::PadTyping;
	using D2RAccess::Strings::Current;
	const auto at = [](Key key) { return KeyAt(key, false); };
	Check(at({}) == 'q', "the keyboard starts on q");
	Check(at(Move({}, 1, 0)) == 'w' && at(Move({}, -1, 0)) == 'p', "left and right step a row, wrapping");
	Check(at(Move({}, 0, -1)) == '1' && at(Move({}, 0, 1)) == 'a', "up and down change row, keeping the column");
	Check(at(Move({1, 9}, 0, 1)) == 'l' && at(Move({2, 8}, 0, 1)) == 'm', "a shorter row takes its last key");
	Check(at(Move({0, 0}, 0, -1)) == CapsLockKey && at(Move({4, 0}, 0, 1)) == '1', "rows wrap");
	Check(at({4, 1}) == ' ' && at({4, 6}) == BackspaceKey, "space, signs and Backspace share the last row");
	Check(KeyAt({1, 0}, true) == 'Q' && KeyAt({0, 0}, true) == '1' && KeyAt({4, 2}, true) == '-',
		"Caps Lock raises letters only");

	SetQwerty(true);
	Reset();
	Tick();
	Check(spoken.size() == 1 && spoken.back() == Current().typingStartedQwerty, "editing begins on the keyboard, said so");
	Tick(false, StickPush::Down);
	Tick(false, StickPush::Right);
	Check(spoken.back() == "s", "the stick walks the keyboard, each key said");
	Tick(false, StickPush::Down);
	Tick(false, StickPush::Down);
	Tick(false, StickPush::Left);
	Check(spoken.back() == Current().capsLockKey, "Caps Lock is named");
	Tick(true);
	Check(spoken.back() == Current().upperCase, "A on Caps Lock turns it on");
	Tick(false, StickPush::Up);
	Check(spoken.back() == "Z", "letters are said in upper case with Caps Lock on");
	Tick(false, StickPush::Down);
	Tick(true);
	Check(spoken.back() == Current().lowerCase, "and A again turns it off");
	Tick(false, StickPush::Left);
	Check(spoken.back() == Current().backspaceKey, "Backspace is named");

	SetQwerty(false);
	Reset();
	spoken.clear();
	Tick();
	Tick(false, StickPush::Up);
	Check(spoken.size() == 2 && spoken[0] == Current().typingStarted && spoken[1] == "b",
		"with the switch off, the stick steps through the character list as before");
	Passed();
}
