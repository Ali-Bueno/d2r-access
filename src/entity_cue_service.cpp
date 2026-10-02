#include "entity_cue_service.h"
#include "async_log.h"
#include "attack_reach.h"
#include "entity_cue_audio.h"
#include "entity_cues.h"
#include "native_attack.h"
#include "native_entities.h"
#include "native_world.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::EntityCueService {
namespace {
using EventKind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, On, Off, Status, NpcOn, NpcOff, EnemyOn, EnemyOff, RangedOn, RangedOff,
	OrdinaryOn, OrdinaryOff, ReachOn, ReachOff, AttackAuto, AttackMelee, AttackRanged };
struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0, revision = 0; int level = 0; };
constexpr uint64_t PollMs = 100;
// Skills and weapons change by hand, at most a few times a minute.
constexpr uint64_t LoadoutMs = 1000;

std::mutex g_mutex;
Lifecycle g_lifecycleState;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::atomic<Command> g_command{Command::None};
EntityCues::Tracker g_tracker;
EntityCues::Settings g_settings;
AttackReach::Settings g_reachSettings;
AttackReach::Tracker g_reachTracker;
std::optional<AttackReach::Style> g_style;
std::optional<double> g_reach;
std::string g_reachLogged;
bool g_enabled = true, g_supported = false, g_audioStarted = false, g_suspended = true, g_reachCue = true;
uint64_t g_revision = 0, g_lastPoll = 0, g_lastLoadout = 0;

void Suspend(const D2RL::PluginContext* context, const char* reason) {
	if (g_suspended) return;
	EntityCueAudio::Silence();
	g_tracker.Reset();
	g_reachTracker.Reset();
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
	else if (arg == "ordinary-on") next = Command::OrdinaryOn;
	else if (arg == "ordinary-off") next = Command::OrdinaryOff;
	else if (arg == "reach-on") next = Command::ReachOn;
	else if (arg == "reach-off") next = Command::ReachOff;
	else if (arg == "attack-auto") next = Command::AttackAuto;
	else if (arg == "attack-melee") next = Command::AttackMelee;
	else if (arg == "attack-ranged") next = Command::AttackRanged;
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
	result += ". "; result += g_settings.ordinary ? strings.entityCueOrdinaryEnabled : strings.entityCueOrdinaryDisabled;
	result += ". "; result += g_reachCue ? strings.entityCueReachEnabled : strings.entityCueReachDisabled;
	if (g_style && g_reach) {
		char reach[160]{};
		std::snprintf(reach, sizeof reach, *g_style == AttackReach::Style::Melee ? strings.attackReachMelee : strings.attackReachRanged,
			EntityRadar::DistancePhrase(*g_reach, {}, strings).c_str());
		result += ". "; result += reach;
		result += ", "; result += g_reachSettings.mode == AttackReach::Mode::Auto ? strings.attackModeAuto : strings.attackModeFixed;
	}
	if (!g_supported) { result += ". "; result += strings.entityCueReaderUnavailable; }
	if (!g_audioStarted || !EntityCueAudio::Available()) { result += ". "; result += strings.entityCueAudioUnavailable; }
	return result;
}

// The reach is published even while the cues are off: the HUD name gate uses it too.
void RefreshReach(const D2RL::PluginContext* context) {
	const auto loadout = NativeAttack::ReadLoadout(context);
	if (!loadout && g_reachSettings.mode == AttackReach::Mode::Auto) { g_style.reset(); g_reach.reset(); return; }
	const auto inHand = loadout.value_or(AttackReach::Loadout{});
	g_style = AttackReach::Resolve(g_reachSettings.mode, inHand);
	g_reach = AttackReach::Reach(*g_style, inHand, g_reachSettings);
	char line[160]{};
	std::snprintf(line, sizeof line, "style=%s reach=%.1f mode=%d skills=%zu missile=%u adder=%d",
		*g_style == AttackReach::Style::Melee ? "melee" : "ranged", *g_reach, static_cast<int>(g_reachSettings.mode),
		inHand.skills.size(), static_cast<unsigned>(inHand.missileWeapon), inHand.rangeAdder);
	if (g_reachLogged == line) return;
	g_reachLogged = line;
	AsyncLog::InfoF(context, "[attack-reach] %s", line);
}
}

std::optional<double> CurrentReach() { return g_reach; }
std::optional<double> NamingReach() {
	return g_style == AttackReach::Style::Melee ? g_reach : std::nullopt;
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
	g_settings.ordinary = config.ordinary.value_or(g_settings.ordinary);
	g_reachCue = config.reach.value_or(g_reachCue);
	g_reachSettings.mode = config.attack.value_or(g_reachSettings.mode);
	g_reachSettings.meleeReach = config.meleeReach.value_or(g_reachSettings.meleeReach);
	g_reachSettings.rangedReach = config.rangedReach.value_or(g_settings.range);
	(void)context->RegisterConsoleCommand("access-entity-cues", Console,
		"Entity cues: on|off|status|npc-on|npc-off|enemy-on|enemy-off|ranged-on|ranged-off|ordinary-on|ordinary-off"
		"|reach-on|reach-off|attack-auto|attack-melee|attack-ranged.");
	g_audioStarted = EntityCueAudio::Start(context, static_cast<double>(config.volume.value_or(100)) / 100.0);
	AsyncLog::InfoF(context, "[entity-cues] registered enabled=%u native=%u audio=%u range=%.1f npc=%u enemy=%u ranged=%u"
		" ordinary=%u reach=%u attack=%d melee=%.1f ranged_reach=%.1f",
		g_enabled, g_supported, g_audioStarted && EntityCueAudio::Available(), g_settings.range,
		g_settings.npcs, g_settings.enemies, g_settings.ranged, g_settings.ordinary, g_reachCue,
		static_cast<int>(g_reachSettings.mode), g_reachSettings.meleeReach, g_reachSettings.rangedReach);
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
	case Command::OrdinaryOn: g_settings.ordinary = true; break;
	case Command::OrdinaryOff: g_settings.ordinary = false; break;
	case Command::ReachOn: g_reachCue = true; break;
	case Command::ReachOff: g_reachCue = false; break;
	case Command::AttackAuto: g_reachSettings.mode = AttackReach::Mode::Auto; break;
	case Command::AttackMelee: g_reachSettings.mode = AttackReach::Mode::Melee; break;
	case Command::AttackRanged: g_reachSettings.mode = AttackReach::Mode::Ranged; break;
	default: break;
	}
	if (command >= Command::AttackAuto) g_lastLoadout = 0;
	if (changed) Suspend(context, "settings changed");
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_lifecycleState; }
	if (state.revision != g_revision) {
		g_revision = state.revision;
		g_tracker.Reset();
		Suspend(context, "lifecycle changed");
		g_lastPoll = g_lastLoadout = 0;
	}
	const uint64_t now = GetTickCount64();
	if (!state.ready || !g_supported) { g_style.reset(); g_reach.reset(); }
	else if (!g_lastLoadout || now - g_lastLoadout >= LoadoutMs) { g_lastLoadout = now; RefreshReach(context); }
	if (command != Command::None) {
		ScreenReader::Speak(StatusText(), true);
		AsyncLog::InfoF(context, "[entity-cues] status enabled=%u native=%u audio=%u range=%.1f npc=%u enemy=%u ranged=%u",
			g_enabled, g_supported, g_audioStarted && EntityCueAudio::Available(), g_settings.range,
			g_settings.npcs, g_settings.enemies, g_settings.ranged);
	}
	if (!g_enabled || !g_supported || !g_audioStarted || !EntityCueAudio::Available() || !state.ready) {
		Suspend(context, "disabled or unavailable"); return;
	}
	if (!RadarUi::InPlay()) { Suspend(context, "UI/focus"); return; }
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
	if (g_reachCue && g_reach) {
		if (const auto target = g_reachTracker.Update(world, entities, *g_reach, g_reachSettings.release)) {
			AsyncLog::InfoF(context, "[attack-reach] in reach id=%u distance=%.2f reach=%.1f",
				target->id, target->bearing.distance, *g_reach);
			EntityCueAudio::Play(EntityCueAudio::Sound::Reach, {target->bearing.right, target->bearing.front});
			return;
		}
	}
	const auto cue = g_tracker.Update(world, entities, now, g_settings);
	if (!cue) return;
	EntityCueAudio::Sound sound = EntityCueAudio::Sound::Npc;
	switch (cue->kind) {
	case EntityCues::Kind::Npc: sound = EntityCueAudio::Sound::Npc; break;
	case EntityCues::Kind::Important: sound = EntityCueAudio::Sound::Important; break;
	case EntityCues::Kind::Resurrector: sound = EntityCueAudio::Sound::Resurrector; break;
	case EntityCues::Kind::Ranged: sound = EntityCueAudio::Sound::Ranged; break;
	case EntityCues::Kind::Ordinary: sound = EntityCueAudio::Sound::Ordinary; break;
	}
	AsyncLog::InfoF(context, "[entity-cues] generated event_ms=%llu id=%u kind=%u slot=%zu distance=%.2f right=%.3f front=%.3f interval_ms=%llu",
		static_cast<unsigned long long>(now), cue->id, static_cast<unsigned>(cue->kind), cue->slot, cue->distance,
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
	g_reachSettings = {}; g_reachTracker.Reset(); g_style.reset(); g_reach.reset(); g_reachLogged.clear(); g_reachCue = true;
	g_revision = g_lastPoll = g_lastLoadout = 0;
}
}
