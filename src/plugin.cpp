// D2RLoader entry points. Wiring only: every behaviour lives in a service.
#include <D2RLPlugin/api.h>

#include "code_dump.h"
#include "focus_tracker.h"
#include "input_direction.h"
#include "localization.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "ui_message_monitor.h"
#include "ui_thread_pump.h"
#include "widgets.h"

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr D2RL::PluginInfo PluginInfo {
	.infoSize    = D2RL::PluginInfoSize,
	.apiVersion  = D2RL_PLUGIN_API_VERSION,
	.id          = "d2r-access",
	.name        = "Diablo II: Resurrected Accessibility",
	.version     = "0.1.0",
	.author      = "d2r-access",
	.description = "Screen reader support for blind players (PRISM).",
	.flags       = D2RL::PluginFlags::Client,
};

// One UI update: sample input first so a value message arriving this update
// can be attributed to the press that caused it, then read focus.
void OnUiTick(const D2RL::PluginContext* context) {
	D2RAccess::InputDirection::Poll();
	D2RAccess::FocusTracker::Tick(context);
	D2RAccess::UiMessageMonitor::Tick(context);
	D2RAccess::NativeWidgets::Tick(context);
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

	if (!D2RAccess::Localization::Initialize(context)) {
		context->LogWarn("Localization service unavailable; panel titles will use internal names.");
	}
	D2RAccess::Localization::DetectSpeechLanguage(context);

	if (D2RAccess::ScreenReader::Initialize(context->pluginDirectory)) {
		D2RL::LogInfoF(context, "PRISM ready, backend: %s", D2RAccess::ScreenReader::BackendName());
		D2RAccess::ScreenReader::Speak(D2RAccess::Strings::Current().pluginLoaded, true);
	} else {
		D2RL::LogErrorF(context, "%s: %s", D2RAccess::Strings::Current().screenReaderMissing, D2RAccess::ScreenReader::LastError());
	}
	if (!D2RAccess::UiMessageMonitor::Register(context)) {
		context->LogError("UI message listener could not be registered.");
		return false;
	}

	(void)D2RAccess::NativeWidgets::Initialize(context);
	if (!D2RAccess::Widgets::Initialize(context)) {
		context->LogWarn("Widget service unavailable; focus tracking disabled.");
	} else if (!D2RAccess::UiThreadPump::Start(context, OnUiTick)) {
		context->LogWarn("Thread service unavailable; focus tracking disabled.");
	}

	(void)context->RegisterConsoleCommand("access-say", SayCommand, "Speak text through the screen reader.");
	(void)context->RegisterConsoleCommand("access-trace", TraceCommand, "Toggle logging of every UI message (on|off).");
	(void)context->RegisterConsoleCommand("access-dump-code", DumpCodeCommand, "Write the in-memory code section next to the plugin.");
	(void)context->RegisterConsoleCommand("access-probe", ProbeCommand, "Log the strings inside a native widget (Panel/Child/...).");
	context->LogInfo("d2r-access loaded.");
	return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	D2RAccess::UiThreadPump::Stop();
	D2RAccess::InputDirection::Shutdown();
	D2RAccess::UiMessageMonitor::Unregister();
	D2RAccess::Widgets::Shutdown();
	D2RAccess::Localization::Shutdown();
	D2RAccess::ScreenReader::Shutdown();
}
