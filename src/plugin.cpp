#include "async_log.h"
// D2RLoader entry points. Wiring only: every behaviour lives in a service.
#include <D2RLPlugin/api.h>

#include "code_dump.h"
#include "core_slots.h"
#include "focus_tracker.h"
#include "footstep_service.h"
#include "game_text_reader.h"
#include "ground_item_service.h"
#include "beacon_service.h"
#include "entity_radar_service.h"
#include "input_direction.h"
#include "input_stack.h"
#include "localization.h"
#include "native_widgets.h"
#include "native_tooltips.h"
#include "native_skill_navigation.h"
#include "skill_navigation.h"
#include "quest_service.h"
#include "radar_service.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "ui_message_monitor.h"
#include "ui_thread_pump.h"
#include "vitals_service.h"
#include "widget_probe.h"
#include "xinput_hook.h"
#include "widgets.h"

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr D2RL::PluginInfo PluginInfo {
	.infoSize    = D2RL::PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = "d2r-access",
	.name        = "Diablo II: Resurrected Accessibility",
	.version     = "0.1.0",
	.author      = "d2r-access",
	.description = "Screen reader support for blind players (PRISM).",
	.flags       = D2RL::PluginFlags::Client | D2RL::PluginFlags::NativeHooks,
};

// One UI update: sample input first so a value message arriving this update
// can be attributed to the press that caused it, then read focus.
void OnUiTick(const D2RL::PluginContext* context) {
	D2RAccess::InputDirection::Poll();
	D2RAccess::XInputHook::Tick(context);
	D2RAccess::SkillNavigation::Tick(context);
	const bool uiOwnsSpeech = D2RAccess::GameTextReader::Tick(context);
	if (!uiOwnsSpeech) D2RAccess::FocusTracker::Tick(context);
	D2RAccess::WidgetProbe::Tick(context);
	D2RAccess::UiMessageMonitor::Tick(context);
	D2RAccess::NativeWidgets::Tick(context);
	D2RAccess::QuestService::Tick(context, uiOwnsSpeech);
	D2RAccess::RadarService::Tick(context);
	D2RAccess::FootstepService::Tick();
	D2RAccess::GroundItemService::Tick(context, uiOwnsSpeech);
	D2RAccess::EntityRadarService::Tick(context, uiOwnsSpeech);
	D2RAccess::BeaconService::Tick(context);
	D2RAccess::VitalsService::Tick(context);
}

auto TextCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command || !command->plugin) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (args == "on") D2RAccess::GameTextReader::SetEnabled(true);
	else if (args == "off") D2RAccess::GameTextReader::SetEnabled(false);
	else if (args == "repeat") D2RAccess::GameTextReader::RequestRepeat();
	else if (args == "dump") D2RAccess::GameTextReader::RequestDump();
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}

// Raw bytes of the FOCUSED widget, unlike access-probe above, which logs the
// strings of a widget named by path. Used to derive an offset the SDK does not
// expose (a checkbox's checked flag, a dropdown's selected row) by comparing
// the same control in two states.
auto BytesCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (args == "on") D2RAccess::WidgetProbe::SetEnabled(true);
	else if (args == "off") D2RAccess::WidgetProbe::SetEnabled(false);
	else if (args == "once") D2RAccess::WidgetProbe::RequestOnce();
	else if (args == "again") D2RAccess::WidgetProbe::Rearm();
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}

// Console: "access-pad" reports how the game polls each controller slot;
// "access-pad off" restores XInputGetState. Nothing is injected either way.
auto PadCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (args == "off") D2RAccess::XInputHook::Remove(command->plugin);
	else if (args.empty() || args == "report") D2RAccess::XInputHook::Report(command->plugin);
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}

auto SkillGridCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command || !command->plugin) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (args == "on") D2RAccess::SkillNavigation::SetEnabled(true);
	else if (args == "off") D2RAccess::SkillNavigation::SetEnabled(false);
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}

// Console: "access-probe Panel/Child/Child" logs the engine strings inside
// that widget's native object.
auto ProbeCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	if (command->args == nullptr || command->argsLength == 0) {
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	std::vector<std::string> steps;
	std::string_view         rest(command->args, command->argsLength);
	while (!rest.empty()) {
		const size_t slash = rest.find('/');
		steps.emplace_back(rest.substr(0, slash));
		rest = slash == std::string_view::npos ? std::string_view {} : rest.substr(slash + 1);
	}
	const std::string panel = steps.front();
	steps.erase(steps.begin());
	D2RAccess::NativeWidgets::RequestProbe(panel, steps, 0);
	return D2RL::ConsoleCommandResult::Handled;
}

// Console: "access-say <text>" speaks the text through the screen reader.
auto SayCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	if (command->args == nullptr || command->argsLength == 0) {
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	D2RAccess::ScreenReader::Speak(command->args, true);
	return D2RL::ConsoleCommandResult::Handled;
}

// Console: "access-dump-code" writes the in-memory .text section for offline analysis.
auto DumpCodeCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	return D2RAccess::CodeDump::EnsureDumped(command->plugin) ? D2RL::ConsoleCommandResult::Handled : D2RL::ConsoleCommandResult::Failed;
}

// Console: "access-trace on|off" toggles logging of every UI message.
auto TraceCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	const char* args = command->args != nullptr ? command->args : "";
	if (std::strcmp(args, "on") == 0) {
		D2RAccess::UiMessageMonitor::SetTraceEnabled(true);
		D2RAccess::FocusTracker::SetDiagnosticsEnabled(true);
	} else if (std::strcmp(args, "off") == 0) {
		D2RAccess::UiMessageMonitor::SetTraceEnabled(false);
		D2RAccess::FocusTracker::SetDiagnosticsEnabled(false);
	} else {
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	command->plugin->WriteConsoleMessage(D2RAccess::UiMessageMonitor::IsTraceEnabled() ? "UI message trace on" : "UI message trace off");
	return D2RL::ConsoleCommandResult::Handled;
}

}

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &PluginInfo;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	if (context == nullptr) {
		return false;
	}

	// Before any mod code touches XInput, so the snapshot records what the
	// GAME loaded rather than what this plugin pulled in.
	D2RAccess::InputStack::Capture();

	if (!D2RAccess::AsyncLog::Initialize()) return false;

	// The native readers depend on what this loader build resolves, so every log
	// starts by naming it: a report that opens with the loader is one nobody has
	// to reconstruct afterwards.
	const char* build = D2RL::GetBuildVersion(context);
	const char* name  = D2RL::GetBuildName(context);
	D2RAccess::AsyncLog::InfoF(context, "d2r-access %s on game %s%s%s, loader %s (plugin ABI %u)", PluginInfo.version,
		build != nullptr ? build : "of unknown build", name != nullptr ? " " : "", name != nullptr ? name : "",
		D2RAccess::CoreSlots::LoaderVersion(), static_cast<unsigned>(PluginInfo.abiVersion));

	D2RAccess::InputStack::Report(context);

	if (!D2RAccess::Localization::Initialize(context)) {
		D2RAccess::AsyncLog::Warn(context, "Localization service unavailable; panel titles will use internal names.");
	}
	D2RAccess::Localization::DetectSpeechLanguage(context);

	if (D2RAccess::ScreenReader::Initialize(context->pluginDirectory)) {
		D2RAccess::AsyncLog::InfoF(context, "PRISM ready, backend: %s", D2RAccess::ScreenReader::BackendName());
		D2RAccess::ScreenReader::Speak(D2RAccess::Strings::Current().pluginLoaded, true);
	} else {
		D2RAccess::AsyncLog::ErrorF(context, "%s: %s", D2RAccess::Strings::Current().screenReaderMissing, D2RAccess::ScreenReader::LastError());
	}
	if (!D2RAccess::UiMessageMonitor::Register(context)) {
		D2RAccess::AsyncLog::Error(context, "UI message listener could not be registered.");
		D2RAccess::ScreenReader::Shutdown();
		D2RAccess::AsyncLog::Shutdown();
		return false;
	}

	(void)D2RAccess::NativeWidgets::Initialize(context);
	(void)D2RAccess::NativeTooltips::Initialize(context);
	(void)D2RAccess::NativeSkillNavigation::Initialize(context);
	if (!D2RAccess::RadarService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[radar] Sonar lifecycle unavailable.");
	D2RAccess::FootstepService::Register(context);
	if (!D2RAccess::GroundItemService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[items] Ground item lifecycle unavailable.");
	if (!D2RAccess::EntityRadarService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[scan] Entity scan lifecycle unavailable.");
	if (!D2RAccess::BeaconService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[beacon] Beacon lifecycle unavailable.");
	if (!D2RAccess::GameTextReader::Register(context)) D2RAccess::AsyncLog::Warn(context, "[text] Gameplay lifecycle unavailable; in-game text reading disabled.");
	if (!D2RAccess::QuestService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[quest] Quest lifecycle unavailable.");
	if (!D2RAccess::VitalsService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[vitals] Life, mana and equipment announcements unavailable.");
	if (!D2RAccess::Widgets::Initialize(context)) {
		D2RAccess::AsyncLog::Warn(context, "Widget service unavailable; focus tracking disabled.");
	} else if (!D2RAccess::UiThreadPump::Start(context, OnUiTick)) {
		D2RAccess::AsyncLog::Warn(context, "Thread service unavailable; focus tracking disabled.");
	}

	(void)context->RegisterConsoleCommand("access-say", SayCommand, "Speak text through the screen reader.");
	(void)context->RegisterConsoleCommand("access-trace", TraceCommand, "Toggle logging of every UI message (on|off).");
	(void)context->RegisterConsoleCommand("access-dump-code", DumpCodeCommand, "Write the in-memory code section next to the plugin.");
	(void)context->RegisterConsoleCommand("access-probe", ProbeCommand, "Log the strings inside a native widget (Panel/Child/...).");
	(void)context->RegisterConsoleCommand("access-text", TextCommand, "In-game text: on|off|repeat|dump (visible native tree).");
	(void)context->RegisterConsoleCommand("access-bytes", BytesCommand, "Focused-widget byte dump: on|off|once|again.");
	(void)context->RegisterConsoleCommand("access-pad", PadCommand, "Controller-slot poll counts: (no args)|report|off.");
	(void)context->RegisterConsoleCommand("access-skill-grid", SkillGridCommand, "D-pad skill grid: on|off.");
	(void)D2RAccess::XInputHook::Install(context);
	D2RAccess::AsyncLog::Info(context, "d2r-access loaded.");
	return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	D2RAccess::XInputHook::Remove(nullptr);
	D2RAccess::UiThreadPump::Stop();
	D2RAccess::VitalsService::Unregister();
	D2RAccess::QuestService::Unregister();
	D2RAccess::SkillNavigation::Reset();
	D2RAccess::BeaconService::Unregister();
	D2RAccess::EntityRadarService::Unregister();
	D2RAccess::GroundItemService::Unregister();
	D2RAccess::FootstepService::Unregister();
	D2RAccess::RadarService::Unregister();
	D2RAccess::GameTextReader::Unregister();
	D2RAccess::InputDirection::Shutdown();
	D2RAccess::UiMessageMonitor::Unregister();
	D2RAccess::Widgets::Shutdown();
	D2RAccess::Localization::Shutdown();
	D2RAccess::ScreenReader::Shutdown();
	D2RAccess::AsyncLog::Shutdown();
}
