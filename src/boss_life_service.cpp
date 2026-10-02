#include "boss_life_service.h"
#include "async_log.h"
#include "boss_life.h"
#include "native_entities.h"
#include "native_status.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::BossLifeService {
namespace {
using EventKind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, On, Off, Status };
struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0, revision = 0; };
// A blow lands in a fraction of a second; a step is spoken within one more poll.
constexpr uint64_t PollMs = 200;

std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::atomic<Command> g_command{Command::None};
BossLife::Tracker g_tracker;
bool g_enabled = true;
uint64_t g_revision = 0, g_lastPoll = 0;

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == EventKind::GameJoined || event->kind == EventKind::GameLeft) g_state.ready = false;
	if (event->kind == EventKind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; }
	++g_state.revision;
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

// Nothing when the units cannot be read, so a failed read never drops a baseline.
std::optional<std::vector<BossLife::Reading>> Read(const Lifecycle& state) {
	std::vector<EntityRadar::Entity> entities;
	if (!NativeEntities::Read(state.player, state.session, entities)) return std::nullopt;
	std::vector<BossLife::Reading> readings;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Enemy || !entity.enemy.boss) continue;
		if (const auto life = NativeStatus::MonsterLife(entity.id)) readings.push_back({entity.id, entity.name, *life});
	}
	// The living units no longer list a boss that has just died; its own unit says so.
	for (const auto& followed : g_tracker.Followed()) {
		if (std::any_of(readings.begin(), readings.end(), [&](const auto& reading) { return reading.id == followed.id; })) continue;
		if (const auto life = NativeStatus::MonsterLife(followed.id); life && *life == 0)
			readings.push_back({followed.id, followed.name, 0});
	}
	return readings;
}

void Announce(const D2RL::PluginContext* context, const std::vector<BossLife::Alert>& alerts) {
	std::string text;
	bool interrupt = false;
	for (const auto& alert : alerts) {
		char phrase[256]{};
		std::snprintf(phrase, sizeof phrase, Strings::Current().bossLife, alert.name.c_str(), alert.percent);
		if (!text.empty()) text += ". ";
		text += phrase;
		interrupt |= alert.interrupt;
	}
	if (text.empty()) return;
	AsyncLog::InfoF(context, "[boss-life] %s", text.c_str());
	ScreenReader::Speak(text, interrupt);
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) { Unregister(); return false; }
	for (auto kind : {EventKind::GameJoined, EventKind::GameLeft, EventKind::LocalPlayerReady,
		EventKind::ActChanged, EventKind::LevelChanged, EventKind::PlayerResurrected}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize,
			.kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	g_enabled = RadarConfig::ReadBossLife(context).enabled.value_or(true);
	(void)context->RegisterConsoleCommand("access-boss-life", Console, "Boss life announcements: on|off|status.");
	AsyncLog::InfoF(context, "[boss-life] registered enabled=%u", g_enabled);
	return true;
}

void Tick(const D2RL::PluginContext* context) {
	const auto command = g_command.exchange(Command::None);
	if (command == Command::On || command == Command::Off) { g_enabled = command == Command::On; g_tracker.Reset(); }
	if (command != Command::None) ScreenReader::Speak(g_enabled ? Strings::Current().bossLifeEnabled : Strings::Current().bossLifeDisabled, true);
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	if (state.revision != g_revision) { g_revision = state.revision; g_tracker.Reset(); }
	// A paused game deals no damage; menus keep the last baseline, so nothing repeats on return.
	if (!g_enabled || !state.ready || !RadarUi::InPlay()) return;
	const uint64_t now = GetTickCount64();
	if (now - g_lastPoll < PollMs) return;
	g_lastPoll = now;
	if (const auto readings = Read(state)) Announce(context, g_tracker.Update(*readings));
}

void Unregister() {
	if (g_lifecycle && g_context) for (auto handle : g_listeners)
		(void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	g_listeners.clear();
	g_tracker.Reset();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	g_context = nullptr; g_lifecycle = nullptr;
	g_command = Command::None;
	g_enabled = true;
	g_revision = g_lastPoll = 0;
}
}
