#include "quest_service.h"
#include "async_log.h"
#include "cairn_scroll_service.h"
#include "localization.h"
#include "layout_check.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "quest_progress.h"
#include "quest_seals.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <cstring>
#include <intrin.h>
#include <mutex>
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
// D2R 3.3.93787: the packet handler at 0x16C570 calls the stock quest-text
// builder after updating quest flags. Capture only that call's completed text.
constexpr uintptr_t BuilderRva = 0x16BD20, PacketHandlerRva = 0x16C570;
constexpr uintptr_t PacketBuilderReturnRva = 0x16D3F0, QuestRecordsRva = 0x1CDA7A0;
constexpr size_t QuestRecordCount = 41, QuestRecordSize = 0x18, QuestRecordTextOffset = 8;
constexpr size_t NativeTextLength = 300, NativeTextOffset = 2;
constexpr uint8_t BuilderPrefix[] = {0x48, 0x89, 0x54, 0x24, 0x10, 0x89, 0x4C, 0x24, 0x08,
	0x55, 0x48, 0x8D, 0x6C, 0x24, 0xA9};
struct CodeCheck { uintptr_t rva; size_t size; uint64_t hash; };
constexpr CodeCheck NativeChecks[] {
	{BuilderRva, 1807, 0xE2BE64710FBA0735ULL},
	{PacketHandlerRva, 4108, 0xCA5D9ABF4A4675EULL},
};

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
QuestProgress::PacketMatcher g_packets;
std::mutex g_packetMutex;
std::atomic<uintptr_t> g_nativeBase{0};
std::atomic<uint64_t> g_captureEpoch{0};
std::atomic<bool> g_nativeReady{false};
void* g_originalBuilder = nullptr;

uintptr_t NativeBuilder(uint32_t row, void* output) noexcept {
	const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
	const uint64_t epoch = g_captureEpoch.load(std::memory_order_acquire);
	const bool ready = g_nativeReady.load(std::memory_order_acquire);
	const uintptr_t result = reinterpret_cast<uintptr_t(*)(uint32_t, void*)>(g_originalBuilder)(row, output);
	const uintptr_t base = g_nativeBase.load(std::memory_order_acquire);
	if (!ready || !base || caller != base + PacketBuilderReturnRva || row >= QuestRecordCount || !output) return result;
	std::array<uint8_t, NativeTextOffset + NativeTextLength> bytes{};
	uintptr_t keys = 0;
	uint16_t titleId = 0, recordTitleId = 0;
	if (!NativeMemory::Copy(reinterpret_cast<uintptr_t>(output), bytes.data(), bytes.size())
		|| !NativeMemory::Peek(base + QuestRecordsRva + row * QuestRecordSize + QuestRecordTextOffset, keys)
		|| !keys || !NativeMemory::Peek(keys, recordTitleId)) return result;
	std::memcpy(&titleId, bytes.data(), sizeof(titleId));
	const char* objective = reinterpret_cast<const char*>(bytes.data() + NativeTextOffset);
	const void* end = std::memchr(objective, 0, NativeTextLength);
	if (titleId != recordTitleId || !end || objective[0] == 0) return result;
	try {
		std::lock_guard lock(g_packetMutex);
		if (ready && g_nativeReady.load(std::memory_order_relaxed)
			&& epoch == g_captureEpoch.load(std::memory_order_relaxed))
			g_packets.Capture({row, titleId, std::string(objective), GetTickCount64()});
	} catch (...) { /* The stock quest update must not depend on our capture. */ }
	return result;
}

void InstallNative(const D2RL::PluginContext* context) {
	if (!context || !context->exeBase) return;
	for (const auto& check : NativeChecks) {
		std::vector<uint8_t> bytes(check.size);
		if (!NativeMemory::Copy(context->exeBase + check.rva, bytes.data(), bytes.size())
			|| LayoutCheck::Hash(bytes) != check.hash) {
			AsyncLog::Warn(context, "[quest] Native packet text unavailable: game code mismatch.");
			return;
		}
	}
	g_nativeBase.store(context->exeBase, std::memory_order_release);
	if (!context->InstallInlineHook(BuilderRva, BuilderPrefix, sizeof(BuilderPrefix),
		reinterpret_cast<void*>(&NativeBuilder), &g_originalBuilder)) {
		g_nativeBase.store(0, std::memory_order_release);
		AsyncLog::Warn(context, "[quest] Native packet text hook unavailable; journal and generic alerts retained.");
		return;
	}
	AsyncLog::Info(context, "[quest] Verified native quest packet text capture installed.");
}

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
	g_ready = false;
	Reset();
	g_session = event->sessionGeneration;
	g_player = event->playerId;
	g_ready = event->kind == Kind::LocalPlayerReady;
	g_nativeReady.store(g_ready && g_enabled, std::memory_order_release);
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

bool Register(const D2RL::PluginContext* context, bool nativeHooks) {
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
	if (nativeHooks) InstallNative(context);
	AsyncLog::Info(context, "[quest] Automatic announcements registered.");
	return true;
}

void Unregister() {
	g_nativeBase.store(0, std::memory_order_release);
	if (g_context && g_lifecycle) for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	g_listeners.clear();
	g_context = nullptr;
	g_lifecycle = nullptr;
	g_ready = false;
	g_command = Command::None;
	CairnScrollService::Reset();
	Reset();
}

void Note(const char* command) {
	if (!g_ready || !g_enabled || !command) return;
	const std::string_view name(command);
	const uint64_t now = GetTickCount64();
	if (name == "SetQuestAlert" || name == "QuestUpdate") {
		g_reader.Alert(now);
		{ std::lock_guard lock(g_packetMutex); g_packets.Alert(now); }
		AsyncLog::InfoF(g_context, "[quest] update event: %s", command);
	} else if (name == "SelectQuest" || name == "SelectTab" || name == "Close") {
		g_reader.Navigate();
		{ std::lock_guard lock(g_packetMutex); g_packets.Reset(); }
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
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	const bool foreground = process == GetCurrentProcessId();
	CairnScrollService::Tick(context, g_session, g_ready, foreground);
	// An opened seal says how many are left; it waits while a menu has the speech.
	if (g_ready && !uiOwnsSpeech)
		if (const auto left = QuestSeals::TakeOpening(g_session)) {
			const std::string text = QuestSeals::LeftText(*left, Strings::Current());
			AsyncLog::InfoF(context, "[quest] seal opened: %s", text.c_str());
			ScreenReader::Speak(text, false);
		}
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
	if (!g_sample.open) {
		std::optional<QuestProgress::NativePacket> packet;
		{ std::lock_guard lock(g_packetMutex); packet = g_packets.Take(now); }
		if (packet) {
			const auto resolve = [](std::string_view key) { return Localization::Text(key); };
			const auto title = Localization::Text(packet->titleId);
			auto native = QuestProgress::Identify(title, title, packet->objective, resolve);
			if (!native.quest.empty()) {
				g_reader.Correlate(std::move(native));
				AsyncLog::InfoF(context, "[quest] packet objective: row=%u title-id=%u", packet->row, packet->titleId);
			}
		}
	}
	const bool canSpeak = !uiOwnsSpeech && RadarUi::Inspect(foreground) == RadarUi::State::Playing;
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
	g_nativeReady.store(false, std::memory_order_release);
	g_reader.Reset();
	{ std::lock_guard lock(g_packetMutex);
		g_captureEpoch.fetch_add(1, std::memory_order_release);
		g_packets.Reset();
	}
	g_last = {};
	g_sample = {};
	g_seen = false;
	g_nextWatch = 0;
	g_language = Strings::Selected();
	g_nativeReady.store(g_ready && g_enabled, std::memory_order_release);
}


// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled() { return g_enabled; }
void SetEnabled(bool enabled) { g_command = enabled ? Command::On : Command::Off; }
}
