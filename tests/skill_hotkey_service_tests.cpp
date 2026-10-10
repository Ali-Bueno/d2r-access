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
std::string SkillName(int skill) { return skill == 106 ? "Zeal" : skill == 220 || skill == 221 ? "Tome of Town Portal" : ""; }
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
void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {}
}

int main() {
	Press(); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken == std::vector<std::string>{"Zeal"}, "a selected keyboard skill announces its game name alone");
	Press(); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 1, "a new press of the same skill stays silent until the skill changes");
	SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 1, "leaving a skill selected does not repeat speech");
	Press(106, true); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 1, "changing casting mode alone does not announce the same skill again");
	Press(220, true); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 2 && spoken.back() == "Tome of Town Portal", "quick cast announces the changed binding, not a restored hand skill");
	for (int cast = 0; cast < 20; ++cast) {
		pending.push_back({cast % 16, 220, cast % 2 == 0, true});
		SkillHotkeyService::Tick(nullptr, false);
	}
	Check(spoken.size() == 2, "repeated quick casts stay silent across ticks, binding slots and hands");
	Press(106, true); SkillHotkeyService::Tick(nullptr, false);
	Press(220, true); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 4 && spoken[2] == "Zeal" && spoken[3] == "Tome of Town Portal",
		"switching skills and switching back each announce once");
	Press(221, true); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 5 && spoken.back() == "Tome of Town Portal",
		"distinct skill ids announce even when their displayed names match");
	Press(-1); Press(999); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 5, "missing names never invent an active skill");
	Press(221, true); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 6, "a successful unnamed skill still marks a change in skill identity");
	Press(-1); Press(221, true); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 6, "an invalid skill id does not reset the last accepted skill");
	for (int gate = 0; gate < 5; ++gate) {
		playing = gate != 0; settings = gate == 1;
		typing = gate == 2 ? std::optional<bool>{true} : gate == 3 ? std::nullopt : std::optional<bool>{false};
		Press(); SkillHotkeyService::Tick(nullptr, gate == 4);
		Check(spoken.size() == 6 && pending.empty(), "menus, settings, chat and UI speech discard gameplay events");
	}
	playing = true; settings = false; typing = false;
	SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 6, "returning to play never speaks a stale menu press");
	Press(221, true); SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 6, "opening menus does not restart announcements of an unchanged skill");
	Press(106, true); Press(106, true); Press(220, true); Press(220, true);
	SkillHotkeyService::Tick(nullptr, false);
	Check(spoken.size() == 8 && spoken[6] == "Zeal" && spoken[7] == "Tome of Town Portal",
		"a queued burst announces each skill transition once");
	Passed();
}
