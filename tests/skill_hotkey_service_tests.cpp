#include "skill_hotkey_service.h"
#include "native_skill_hotkeys.h"
#include "test_support.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

using namespace D2RAccess;
namespace {
std::vector<NativeSkillHotkeys::Event> pending;
std::vector<std::string> spoken;
bool playing = true, settings = false;
std::optional<bool> typing = false;
void Press(int skill = 106, bool quick = false) { pending.push_back({0, skill, true, quick}); }
}
namespace D2RAccess::NativeSkillHotkeys {
std::vector<Event> Take() { return std::exchange(pending, {}); }
}
namespace D2RAccess::NativeSkillBar {
std::string SkillName(int skill) { return skill == 106 ? "Zeal" : skill == 220 ? "Tome of Town Portal" : ""; }
}
namespace D2RAccess::KeyboardService {
bool Playing() { return playing; }
bool SettingsOpen() { return settings; }
}
namespace D2RAccess::NativeSkillNavigation {
std::optional<bool> TextEditing() { return typing; }
}
namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool interrupt) {
	Check(interrupt, "a direct hotkey answers the player promptly");
	spoken.emplace_back(text);
}
}
namespace D2RAccess::AsyncLog {
void InfoF(const D2RL::PluginContext*, const char*, ...) {}
}

int main() {
	Press(); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken == std::vector<std::string>{"Zeal"}, "a selected keyboard skill announces its game name alone");
	Press(); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 2 && spoken.back() == "Zeal", "a new press of the already selected skill speaks again");
	SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 2, "leaving a skill selected does not repeat speech");
	Press(220, true); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.back() == "Tome of Town Portal", "quick cast announces the pressed binding, not a restored hand skill");
	Press(-1); Press(999); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 3, "missing names never invent an active skill");
	for (int gate = 0; gate < 5; ++gate) {
		playing = gate != 0; settings = gate == 1;
		typing = gate == 2 ? std::optional<bool>{true} : gate == 3 ? std::nullopt : std::optional<bool>{false};
		Press(); SkillHotkeyService::Tick(nullptr, gate == 4);
		Check(spoken.size() == 3 && pending.empty(), "menus, settings, chat and UI speech discard gameplay events");
	}
	playing = true; settings = false; typing = false;
	SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 3, "returning to play never speaks a stale menu press");
	Passed();
}
