#include "entity_cue_service.h"
#include "async_log.h"
#include "entity_cue_audio.h"
#include "entity_cues.h"
#include "native_entities.h"
#include "native_world.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::EntityCueService {
namespace {
using EventKind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, On, Off, Status, NpcOn, NpcOff, EnemyOn, EnemyOff, RangedOn, RangedOff };
struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0, revision = 0; int level = 0; };
constexpr uint64_t PollMs = 100;

std::mutex g_mutex;
Lifecycle g_lifecycleState;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::atomic<Command> g_command{Command::None};
EntityCues::Tracker g_tracker;
EntityCues::Settings g_settings;
bool g_enabled = true, g_supported = false, g_audioStarted = false, g_suspended = true;
uint64_t g_revision = 0, g_lastPoll = 0;

void Suspend(const D2RL::PluginContext* context, const char* reason) {
	if (g_suspended) return;
	EntityCueAudio::Silence();
	g_tracker.Reset();
	AsyncLog::InfoF(context, "[entity-cues] suspended: %s", reason);
	g_suspended = true;
}

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_lifecycleState.session = event->sessionGeneration;
	if (event->kind == EventKind::GameJoined || event->kind == EventKind::GameLeft) {
		g_lifecycleState.ready = false; g_lifecycleState.level = 0;
	}
	if (event->kind == EventKind::LocalPlayerReady) {
		g_lifecycleState.ready = true;
		g_lifecycleState.player = event->playerId;
		g_lifecycleState.level = 0;
	}
	if (event->kind == EventKind::ActChanged) g_lifecycleState.level = 0;
	if (event->kind == EventKind::LevelChanged) g_lifecycleState.level = event->currentValue;
	++g_lifecycleState.revision;
	EntityCueAudio::Silence();
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	Command next = Command::None;
	if (arg == "on") next = Command::On;
	else if (arg == "off") next = Command::Off;
	else if (arg == "status") next = Command::Status;
	else if (arg == "npc-on") next = Command::NpcOn;
	else if (arg == "npc-off") next = Command::NpcOff;
	else if (arg == "enemy-on") next = Command::EnemyOn;
	else if (arg == "enemy-off") next = Command::EnemyOff;
	else if (arg == "ranged-on") next = Command::RangedOn;
	else if (arg == "ranged-off") next = Command::RangedOff;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	g_command = next;
	return D2RL::ConsoleCommandResult::Handled;
}

std::string StatusText() {
	const auto& strings = Strings::Current();
	std::string result = g_enabled ? strings.entityCuesEnabled : strings.entityCuesDisabled;
	result += ". "; result += g_settings.npcs ? strings.entityCueNpcsEnabled : strings.entityCueNpcsDisabled;
	result += ". "; result += g_settings.enemies ? strings.entityCueEnemiesEnabled : strings.entityCueEnemiesDisabled;
	result += ". "; result += g_settings.ranged ? strings.entityCueRangedEnabled : strings.entityCueRangedDisabled;
	if (!g_supported) { result += ". "; result += strings.entityCueReaderUnavailable; }
	if (!g_audioStarted || !EntityCueAudio::Available()) { result += ". "; result += strings.entityCueAudioUnavailable; }
	return result;
}

// A change says only the switch it changed (the user, 2026-09-28: "just the one
// that was toggled"); status reads them all. Also the phrase the settings list
// holds back when it puts a saved switch back at start.
std::string ChangeText(Command command) {
	const auto& strings = Strings::Current();
	switch (command) {
	case Command::On: case Command::Off: return g_enabled ? strings.entityCuesEnabled : strings.entityCuesDisabled;
	case Command::NpcOn: case Command::NpcOff: return g_settings.npcs ? strings.entityCueNpcsEnabled : strings.entityCueNpcsDisabled;
	case Command::EnemyOn: case Command::EnemyOff: return g_settings.enemies ? strings.entityCueEnemiesEnabled : strings.entityCueEnemiesDisabled;
	default: return g_settings.ranged ? strings.entityCueRangedEnabled : strings.entityCueRangedDisabled;
	}
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_supported = NativeWorld::Initialize(context) && NativeEntities::Initialize(context);
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
	const auto config = RadarConfig::ReadEntityCues(context);
	g_enabled = config.enabled.value_or(true);
	g_settings.npcs = config.npcs.value_or(g_settings.npcs);
	g_settings.enemies = config.enemies.value_or(g_settings.enemies);
	g_settings.ranged = config.ranged.value_or(g_settings.ranged);
	g_settings.range = config.range.value_or(g_settings.range);
	(void)context->RegisterConsoleCommand("access-entity-cues", Console,
		"Entity cues: on|off|status|npc-on|npc-off|enemy-on|enemy-off|ranged-on|ranged-off.");
	g_audioStarted = EntityCueAudio::Start(context, static_cast<double>(config.volume.value_or(100)) / 100.0);
	AsyncLog::InfoF(context, "[entity-cues] registered enabled=%u native=%u audio=%u range=%.1f npc=%u enemy=%u ranged=%u",
		g_enabled, g_supported, g_audioStarted && EntityCueAudio::Available(), g_settings.range,
		g_settings.npcs, g_settings.enemies, g_settings.ranged);
	return true;
}

void Tick(const D2RL::PluginContext* context) {
	const Command command = g_command.exchange(Command::None);
	const bool changed = command != Command::None && command != Command::Status;
	switch (command) {
	case Command::On: g_enabled = true; break;
	case Command::Off: g_enabled = false; break;
	case Command::NpcOn: g_settings.npcs = true; break;
	case Command::NpcOff: g_settings.npcs = false; break;
	case Command::EnemyOn: g_settings.enemies = true; break;
	case Command::EnemyOff: g_settings.enemies = false; break;
	case Command::RangedOn: g_settings.ranged = true; break;
	case Command::RangedOff: g_settings.ranged = false; break;
	default: break;
	}
	if (changed) Suspend(context, "settings changed");
	if (command != Command::None) {
		ScreenReader::Speak(changed ? ChangeText(command) : StatusText(), true);
		AsyncLog::InfoF(context, "[entity-cues] status enabled=%u native=%u audio=%u range=%.1f npc=%u enemy=%u ranged=%u",
			g_enabled, g_supported, g_audioStarted && EntityCueAudio::Available(), g_settings.range,
			g_settings.npcs, g_settings.enemies, g_settings.ranged);
	}
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_lifecycleState; }
	if (state.revision != g_revision) {
		g_revision = state.revision;
		g_tracker.Reset();
		Suspend(context, "lifecycle changed");
		g_lastPoll = 0;
	}
	if (!g_enabled || !g_supported || !g_audioStarted || !EntityCueAudio::Available() || !state.ready) {
		Suspend(context, "disabled or unavailable"); return;
	}
	if (!RadarUi::InPlay()) { Suspend(context, "UI/focus"); return; }
	const uint64_t now = GetTickCount64();
	if (now - g_lastPoll < PollMs) return;
	g_lastPoll = now;
	Radar::World world;
	std::vector<EntityRadar::Entity> entities;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)
		|| !NativeEntities::Read(state.player, state.session, entities)) {
		Suspend(context, "world unavailable"); return;
	}
	if (g_suspended) AsyncLog::Info(context, "[entity-cues] resumed");
	g_suspended = false;
	const auto cue = g_tracker.Update(world, entities, now, g_settings);
	if (!cue) return;
	EntityCueAudio::Sound sound = EntityCueAudio::Sound::Npc;
	switch (cue->kind) {
	case EntityCues::Kind::Npc: sound = EntityCueAudio::Sound::Npc; break;
	case EntityCues::Kind::Important: sound = EntityCueAudio::Sound::Important; break;
	case EntityCues::Kind::Resurrector: sound = EntityCueAudio::Sound::Resurrector; break;
	case EntityCues::Kind::Ranged: sound = EntityCueAudio::Sound::Ranged; break;
	}
	AsyncLog::InfoF(context, "[entity-cues] generated event_ms=%llu id=%u kind=%u distance=%.2f right=%.3f front=%.3f interval_ms=%llu",
		static_cast<unsigned long long>(now), cue->id, static_cast<unsigned>(cue->kind), cue->distance,
		cue->right, cue->front, static_cast<unsigned long long>(cue->intervalMs));
	EntityCueAudio::Play(sound, {cue->right, cue->front});
}

void Unregister() {
	if (g_lifecycle && g_context) for (auto handle : g_listeners)
		(void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	g_listeners.clear();
	EntityCueAudio::Stop();
	g_tracker.Reset();
	{ std::lock_guard lock(g_mutex); g_lifecycleState = {}; }
	g_context = nullptr; g_lifecycle = nullptr;
	g_command = Command::None;
	g_settings = {}; g_enabled = true; g_supported = g_audioStarted = false; g_suspended = true;
	g_revision = g_lastPoll = 0;
}

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled(Cue cue) {
	return cue == Cue::All ? g_enabled : cue == Cue::Npcs ? g_settings.npcs
		: cue == Cue::Enemies ? g_settings.enemies : g_settings.ranged;
}
void SetEnabled(Cue cue, bool enabled) {
	g_command = cue == Cue::All ? (enabled ? Command::On : Command::Off)
		: cue == Cue::Npcs ? (enabled ? Command::NpcOn : Command::NpcOff)
		: cue == Cue::Enemies ? (enabled ? Command::EnemyOn : Command::EnemyOff)
		: (enabled ? Command::RangedOn : Command::RangedOff);
}
}
