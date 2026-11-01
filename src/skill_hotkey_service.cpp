#include "skill_hotkey_service.h"
#include "async_log.h"
#include "keyboard_service.h"
#include "native_ground_items.h"
#include "native_skill_bar.h"
#include "native_skill_hotkeys.h"
#include "native_skill_navigation.h"
#include "radar_config.h"
#include "screen_reader.h"
#include "skill_hotkey.h"

#include <atomic>
#include <string_view>

namespace D2RAccess::SkillHotkeyService {
namespace {
std::atomic<bool> g_enabled = true;
bool g_supported = false;
SkillHotkey::Tracker g_tracker;
uintptr_t g_player = 0;

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command || !command->plugin) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (args == "on") g_enabled = true;
	else if (args == "off") g_enabled = false;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context, bool nativeHooks) {
	if (!context) return false;
	g_enabled = RadarConfig::ReadEnabled(context, "[skill_hotkey]").value_or(true);
	(void)context->RegisterConsoleCommand("access-skill-hotkeys", Console, "Name the skill a keyboard skill hotkey selects: on|off.");
	g_supported = nativeHooks && NativeSkillHotkeys::Initialize(context);
	return g_supported;
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	if (!g_supported) return;
	const auto events = NativeSkillHotkeys::Take();
	// A new game may start on the skill the last one ended on.
	uintptr_t player = 0, tables = 0;
	if (NativeGroundItems::RuntimeRoots(player, tables) && player != g_player) {
		g_player = player;
		g_tracker.Reset();
	}
	if (events.empty() || !g_enabled || uiOwnsSpeech || !KeyboardService::Playing() || KeyboardService::SettingsOpen()
		|| NativeSkillNavigation::TextEditing().value_or(false)) return;
	for (const auto& event : events) {
		if (!g_tracker.Changed(event.skill)) continue;
		const auto name = NativeSkillBar::SkillName(event.skill);
		if (name.empty()) continue;
		AsyncLog::InfoF(context, "[skill-hotkey] slot=%d skill=%d quick-cast=%d -> \"%s\"",
			event.slot, event.skill, event.quickCast ? 1 : 0, name.c_str());
		ScreenReader::Speak(name, true);
	}
}

bool Enabled() { return g_enabled.load(); }
void SetEnabled(bool enabled) { g_enabled = enabled; }
}
