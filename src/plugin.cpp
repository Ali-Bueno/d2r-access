#include "async_log.h"
#include "auto_detail.h"
#include "automap_service.h"
// D2RLoader entry points. Wiring only: every behaviour lives in a service.
#include <D2RLPlugin/api.h>

#include "code_dump.h"
#include "core_slots.h"
#include "focus_tracker.h"
#include "footstep_service.h"
#include "wall_slide_service.h"
#include "game_text_reader.h"
#include "ground_item_service.h"
#include "beacon_service.h"
#include "entity_radar_service.h"
#include "entity_cue_service.h"
#include "input_direction.h"
#include "localization.h"
#include "modal_reader.h"
#include "multiplayer_reader.h"
#include "menu_navigation.h"
#include "native_widgets.h"
#include "native_cairn.h"
#include "native_tooltips.h"
#include "map_survey_service.h"
#include "native_skill_navigation.h"
#include "skill_navigation.h"
#include "skill_bar_service.h"
#include "quest_service.h"
#include "radar_config.h"
#include "room_service.h"
#include "radar_service.h"
#include "terrain_service.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "ui_message_monitor.h"
#include "ui_thread_pump.h"
#include "tab_highlight.h"
#include "vitals_service.h"
#include "weapon_swap_service.h"
#include "trade_service.h"
#include "text_field.h"
#include "widget_probe.h"
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

// One line per right-stick push, once every reader has had its turn, naming
// the one that took it. Four readers claim from the same latch in the order
// below and only one can win, so a push taken by a reader with nothing to say
// is spent in silence -- and that is what the player hears as a push that did
// not fire. Nothing recorded which reader won until this line.
void LogRightStickPush(const D2RL::PluginContext* context) {
	using D2RAccess::InputDirection::StickPush;
	const auto record = D2RAccess::InputDirection::TakePushRecord();
	if (record.push == StickPush::None) return;
	D2RAccess::AsyncLog::InfoF(context, "[stick] %s -> %s",
		D2RAccess::InputDirection::PushName(record.push),
		record.claimant ? record.claimant : "nobody, a later push replaced it");
}

// One UI update: sample input first so a value message arriving this update
// can be attributed to the press that caused it, then read focus.
void OnUiTick(const D2RL::PluginContext* context) {
	D2RAccess::InputDirection::Poll();
	if (!D2RAccess::MenuNavigation::Tick(context)) D2RAccess::SkillNavigation::Tick(context);
	const bool uiOwnsSpeech = D2RAccess::GameTextReader::Tick(context);
	if (!uiOwnsSpeech) D2RAccess::FocusTracker::Tick(context);
	D2RAccess::MultiplayerReader::Tick(context);
	// After the focus tracker, so landing on a strip is its announcement and
	// only a later move along the strip is this one's.
	D2RAccess::TabHighlight::Tick(context, uiOwnsSpeech);
	D2RAccess::TextField::Tick(context);
	D2RAccess::AutoDetail::Tick(context);
	D2RAccess::ModalReader::Tick(context);
	D2RAccess::WidgetProbe::Tick(context);
	D2RAccess::UiMessageMonitor::Tick(context);
	D2RAccess::NativeWidgets::Tick(context);
	D2RAccess::QuestService::Tick(context, uiOwnsSpeech);
	D2RAccess::RadarService::Tick(context);
	D2RAccess::RoomService::Tick(context, uiOwnsSpeech);
	D2RAccess::TerrainService::Tick(context, uiOwnsSpeech);
	D2RAccess::FootstepService::Tick();
	D2RAccess::WallSlideService::Tick();
	D2RAccess::GroundItemService::Tick(context, uiOwnsSpeech);
	D2RAccess::MapSurveyService::Tick(context, uiOwnsSpeech);
	D2RAccess::AutomapService::Tick(context);
	D2RAccess::EntityRadarService::Tick(context, uiOwnsSpeech);
	D2RAccess::EntityCueService::Tick(context);
	D2RAccess::BeaconService::Tick(context);
	D2RAccess::SkillBarService::Tick(context);
	D2RAccess::VitalsService::Tick(context);
	D2RAccess::WeaponSwapService::Tick(context);
	D2RAccess::TradeService::Tick(context);
	LogRightStickPush(context);
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
	// "image" writes the whole mapping; bare or "text" keeps the .text-only
	// dump the fingerprint tooling reads, so neither call site changes size
	// under the other one's feet.
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	const bool ok = args == "image" ? D2RAccess::CodeDump::EnsureImageDumped(command->plugin)
		: D2RAccess::CodeDump::EnsureDumped(command->plugin);
	return ok ? D2RL::ConsoleCommandResult::Handled : D2RL::ConsoleCommandResult::Failed;
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

	if (!D2RAccess::AsyncLog::Initialize()) return false;

	// The native readers depend on what this loader build resolves, so every log
	// starts by naming it: a report that opens with the loader is one nobody has
	// to reconstruct afterwards.
	const char* build = D2RL::GetBuildVersion(context);
	const char* name  = D2RL::GetBuildName(context);
	D2RAccess::AsyncLog::InfoF(context, "d2r-access %s on game %s%s%s, loader %s (plugin ABI %u)", PluginInfo.version,
		build != nullptr ? build : "of unknown build", name != nullptr ? " " : "", name != nullptr ? name : "",
		D2RAccess::CoreSlots::LoaderVersion(), static_cast<unsigned>(PluginInfo.abiVersion));

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
	D2RAccess::MultiplayerReader::Initialize(context);
	D2RAccess::MenuNavigation::Initialize(context);
	// The native switch disables code hooks and input filtering together;
	// readers that only inspect memory remain available.
	const bool nativeHooks = D2RAccess::RadarConfig::ReadEnabled(context, "[native]").value_or(true);
	if (nativeHooks) {
		if (!D2RAccess::NativeCairn::Register(context)) D2RAccess::AsyncLog::Warn(context, "[cairn] Scroll order capture unavailable.");
		(void)D2RAccess::NativeTooltips::Initialize(context);
		(void)D2RAccess::NativeSkillNavigation::Initialize(context);
	} else {
		D2RAccess::AsyncLog::Warn(context, "[native] code hooks disabled by config: no tooltip, cursor, footfall or movement hook this session.");
	}
	if (!D2RAccess::TradeService::Register(context, nativeHooks)) D2RAccess::AsyncLog::Warn(context, "[trade] Trade announcements unavailable.");
	if (!D2RAccess::RadarService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[radar] Sonar lifecycle unavailable.");
	if (nativeHooks) D2RAccess::FootstepService::Register(context);
	D2RAccess::WallSlideService::Register(context, nativeHooks);
	if (!D2RAccess::GroundItemService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[items] Ground item lifecycle unavailable.");
	// After the ground items: the automap reader finds the player through their verified roots.
	if (!D2RAccess::AutomapService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[automap] Automap reader unavailable.");
	if (!D2RAccess::EntityRadarService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[scan] Entity scan lifecycle unavailable.");
	if (!D2RAccess::EntityCueService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[entity-cues] Entity cue lifecycle unavailable.");
	if (!D2RAccess::BeaconService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[beacon] Beacon lifecycle unavailable.");
	if (!D2RAccess::RoomService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[room] Room exit announcements unavailable.");
	if (!D2RAccess::TerrainService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[terrain] Terrain overview unavailable.");
	if (!D2RAccess::GameTextReader::Register(context)) D2RAccess::AsyncLog::Warn(context, "[text] Gameplay lifecycle unavailable; in-game text reading disabled.");
	if (!D2RAccess::QuestService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[quest] Quest lifecycle unavailable.");
	if (!D2RAccess::MapSurveyService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[survey] Map catalogue unavailable.");
	if (!D2RAccess::VitalsService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[vitals] Life, mana and equipment announcements unavailable.");
	if (!D2RAccess::SkillBarService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[skill-bar] LT bar reading unavailable.");
	if (!D2RAccess::WeaponSwapService::Register(context)) D2RAccess::AsyncLog::Warn(context, "[weapon-swap] Weapon swap announcements unavailable.");
	if (!D2RAccess::AutoDetail::Register(context)) D2RAccess::AsyncLog::Warn(context, "[detail] Ctrl+D could not be registered.");
	if (!D2RAccess::Widgets::Initialize(context)) {
		D2RAccess::AsyncLog::Warn(context, "Widget service unavailable; focus tracking disabled.");
	} else if (!D2RAccess::UiThreadPump::Start(context, OnUiTick)) {
		D2RAccess::AsyncLog::Warn(context, "Thread service unavailable; focus tracking disabled.");
	}

	(void)context->RegisterConsoleCommand("access-say", SayCommand, "Speak text through the screen reader.");
	(void)context->RegisterConsoleCommand("access-trace", TraceCommand, "Toggle logging of every UI message (on|off).");
	(void)context->RegisterConsoleCommand("access-dump-code", DumpCodeCommand, "Write the in-memory code next to the plugin: (text)|image.");
	(void)context->RegisterConsoleCommand("access-probe", ProbeCommand, "Log the strings inside a native widget (Panel/Child/...).");
	(void)context->RegisterConsoleCommand("access-text", TextCommand, "In-game text: on|off|repeat|dump (visible native tree).");
	(void)context->RegisterConsoleCommand("access-bytes", BytesCommand, "Focused-widget byte dump: on|off|once|again.");
	(void)context->RegisterConsoleCommand("access-skill-grid", SkillGridCommand, "D-pad skill grid: on|off.");
	// Armed from the config file because the developer console cannot be opened
	// outside a loaded game, and the screens worth dumping are the ones where it
	// cannot: character select and character create.
	if (D2RAccess::RadarConfig::ReadEnabled(context, "[dump]").value_or(false)) {
		if (D2RAccess::CodeDump::EnsureImageDumped(context)) {
			D2RAccess::AsyncLog::Info(context, "[dump] whole-image dump present next to the plugin.");
		} else {
			D2RAccess::AsyncLog::Warn(context, "[dump] whole-image dump failed; see the error above.");
		}
	}
	if (D2RAccess::RadarConfig::ReadEnabled(context, "[probe]").value_or(false)) {
		D2RAccess::WidgetProbe::SetEnabled(true);
		D2RAccess::AsyncLog::Info(context, "[probe] armed from config.");
	}
	D2RAccess::AsyncLog::Info(context, "d2r-access loaded.");
	return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	D2RAccess::UiThreadPump::Stop();
	D2RAccess::VitalsService::Unregister();
	D2RAccess::AutoDetail::Unregister();
	D2RAccess::QuestService::Unregister();
	D2RAccess::NativeCairn::Unregister();
	D2RAccess::SkillNavigation::Reset();
	D2RAccess::MenuNavigation::Reset();
	D2RAccess::TerrainService::Unregister();
	D2RAccess::RoomService::Unregister();
	D2RAccess::BeaconService::Unregister();
	D2RAccess::EntityRadarService::Unregister();
	D2RAccess::EntityCueService::Unregister();
	D2RAccess::GroundItemService::Unregister();
	D2RAccess::AutomapService::Unregister();
	D2RAccess::FootstepService::Unregister();
	D2RAccess::WallSlideService::Unregister();
	D2RAccess::RadarService::Unregister();
	D2RAccess::GameTextReader::Unregister();
	D2RAccess::MultiplayerReader::Reset();
	D2RAccess::InputDirection::Shutdown();
	D2RAccess::UiMessageMonitor::Unregister();
	D2RAccess::Widgets::Shutdown();
	D2RAccess::Localization::Shutdown();
	D2RAccess::ScreenReader::Shutdown();
	D2RAccess::AsyncLog::Shutdown();
}
