#include "quest_service.h"
#include "async_log.h"
#include "localization.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "quest_progress.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

namespace D2RAccess::QuestService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, On, Off, Status };
// Stock layout names and child paths; both game modes share QuestLogPanel.
constexpr const char* PanelNames[] = {"QuestLogPanelExpansion", "QuestLogPanelOriginal"};
const std::string TitlePath[] = {"Title"};
const std::string ShortPath[] = {"ShortDescription"};
const std::string LongPath[] = {"LongDescription"};
constexpr uint64_t WatchMs = 500; // Background objective/counter sampling cadence.

struct Reading {
	std::string panel, title, shortText, longText;
	bool open = false;
	bool operator==(const Reading&) const = default;
};
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::atomic<Command> g_command{Command::None};
bool g_ready = false, g_enabled = true, g_seen = false;
uint64_t g_session = 0, g_nextWatch = 0;
uint32_t g_player = 0;
Strings::Language g_language = Strings::Language::enUS;
Reading g_last;
QuestProgress::Reading g_sample;
QuestProgress::Reader g_reader;

Reading Read() {
	Reading result;
	if (!NativeWidgets::IsAvailable()) return result;
	const NativeMemory::ReadScope scope;
	const auto& openPanels = PanelState::OpenPanels();
	for (const char* name : PanelNames) {
		const uintptr_t panel = NativeWidgets::FindPanel(name);
		if (!panel) continue;
		const bool open = NativeUi::Visible(panel)
			&& std::find(openPanels.begin(), openPanels.end(), name) != openPanels.end();
		if (!result.panel.empty() && !open) continue;
		result = {name, NativeWidgets::TextAt(name, TitlePath), NativeWidgets::TextAt(name, ShortPath),
			NativeWidgets::TextAt(name, LongPath), open};
		if (open) break;
	}
	return result;
}

void __cdecl OnGameplay(const D2RL::PluginContext* context, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	if (event->kind == Kind::QuestCompleted) {
		if (!g_ready || !g_enabled || event->sessionGeneration != g_session || event->playerId != g_player) return;
		g_reader.Completed(event->questRecordId, event->difficulty, GetTickCount64());
		AsyncLog::InfoF(context, "[quest] primary goal done: row=%u difficulty=%u", event->questRecordId, event->difficulty);
		return;
	}
	Reset();
	g_session = event->sessionGeneration;
	g_player = event->playerId;
	g_ready = event->kind == Kind::LocalPlayerReady;
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") g_command = Command::On;
	else if (arg == "off") g_command = Command::Off;
	else if (arg == "status") g_command = Command::Status;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	g_context = context;
	if (!context || context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) { Unregister(); return false; }
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::QuestCompleted}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize,
			.kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	(void)context->RegisterConsoleCommand("access-quests", Console, "Automatic quest announcements: on|off|status.");
	AsyncLog::Info(context, "[quest] Automatic announcements registered.");
	return true;
}

void Unregister() {
	if (g_context && g_lifecycle) for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	g_listeners.clear();
	g_context = nullptr;
	g_lifecycle = nullptr;
	g_ready = false;
	g_command = Command::None;
	Reset();
}

void Note(const char* command) {
	if (!g_ready || !g_enabled || !command) return;
	const std::string_view name(command);
	const uint64_t now = GetTickCount64();
	if (name == "SetQuestAlert" || name == "QuestUpdate") {
		g_reader.Alert(now);
		AsyncLog::InfoF(g_context, "[quest] update event: %s", command);
	} else if (name == "SelectQuest" || name == "SelectTab" || name == "Close") {
		g_reader.Navigate();
		g_sample = {};
		g_seen = false;
	}
	g_nextWatch = now + QuestProgress::SettleMs;
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	if (command == Command::On || command == Command::Off) {
		g_enabled = command == Command::On;
		Reset();
	}
	if (command != Command::None)
		ScreenReader::Speak(g_enabled ? Strings::Current().questAlertsEnabled : Strings::Current().questAlertsDisabled, true);
	if (!g_ready || !g_enabled) return;
	if (g_language != Strings::Selected()) Reset(); // Never announce a locale change as progress.
	const uint64_t now = GetTickCount64();
	if (now >= g_nextWatch) {
		g_nextWatch = now + WatchMs;
		const Reading reading = Read();
		if (!g_seen || reading != g_last) {
			const auto resolve = [](std::string_view key) { return Localization::Text(key); };
			g_sample = QuestProgress::Identify(reading.title, reading.shortText, reading.longText, resolve);
			g_sample.open = reading.open;
			AsyncLog::InfoF(context, "[quest] watch: %s %s | title=%s short=%s long=%s identified=%u",
				reading.panel.empty() ? "no panel" : reading.panel.c_str(), reading.open ? "open" : "closed",
				reading.title.c_str(), reading.shortText.c_str(), reading.longText.c_str(), !g_sample.quest.empty());
			g_last = reading;
			g_seen = true;
		}
	}
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	const bool canSpeak = !uiOwnsSpeech && RadarUi::Inspect(process == GetCurrentProcessId()) == RadarUi::State::Playing;
	const auto announcement = g_reader.Update(g_sample, now, canSpeak);
	std::string text;
	if (announcement.kind == QuestProgress::Kind::Updated) text = Strings::Current().questUpdated;
	else if (announcement.kind == QuestProgress::Kind::GoalDone) text = Strings::Current().questGoalDone;
	else if (announcement.kind == QuestProgress::Kind::Objective) text = announcement.quest + ". " + announcement.objective;
	if (!text.empty()) {
		AsyncLog::InfoF(context, "[quest] announce: %s", text.c_str());
		ScreenReader::Speak(text, false);
	}
}

void Reset() {
	g_reader.Reset();
	g_last = {};
	g_sample = {};
	g_seen = false;
	g_nextWatch = 0;
	g_language = Strings::Selected();
}

}