#include "skill_hotkey_service.h"
#include "async_log.h"
#include "keyboard_service.h"
#include "native_skill_bar.h"
#include "native_skill_hotkeys.h"
#include "native_skill_navigation.h"
#include "screen_reader.h"

namespace D2RAccess::SkillHotkeyService {
namespace {
// Quick cast temporarily restores the hand skill after each cast. Track the
// accepted hotkey's identity instead, without a speech timeout or text dedup.
int g_lastSkill = -1;
}
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	const auto events = NativeSkillHotkeys::Take();
	if (events.empty() || uiOwnsSpeech || !KeyboardService::Playing() || KeyboardService::SettingsOpen()
		|| NativeSkillNavigation::TextEditing().value_or(true)) return;
	for (const auto& event : events) {
		if (event.skill < 0 || event.skill == g_lastSkill) continue;
		g_lastSkill = event.skill;
		const auto name = NativeSkillBar::SkillName(event.skill);
		if (name.empty()) continue;
		AsyncLog::InfoF(context, "[skill-hotkey] slot=%d hand=%s skill=%d quick-cast=%d -> \"%s\"",
			event.slot, event.left ? "left" : "right", event.skill, event.quickCast ? 1 : 0, name.c_str());
		ScreenReader::Speak(name, true);
	}
}
}
