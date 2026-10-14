#include "entity_cue_service.h"
#include "async_log.h"
#include "entity_cue_audio.h"
#include "entity_cues.h"
#include "attack_reach.h"
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
enum class Command { None, On, Off, Status, NpcOn, NpcOff, EnemyOn, EnemyOff, RangedOn, RangedOff, OrdinaryOn, OrdinaryOff, ReachOn, ReachOff, AttackAuto, AttackMelee, AttackRanged };
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
AttackReach::Settings g_reachSettings;
AttackReach::Tracker g_reachTracker;
std::optional<AttackReach::Style> g_style;
std::optional<double> g_reach;
std::string g_reachLogged;
bool g_reachCue = true;
uint64_t g_lastLoadout = 0;
constexpr uint64_t LoadoutMs = 1000;
bool g_enabled = true, g_supported = false, g_audioStarted = false, g_suspended = true;
std::atomic<bool> g_npcAlerts{true};
EntityCues::AlertTracker g_alerts;
std::mutex g_alertMutex; // the announcer reads the tracker too
uint64_t g_revision = 0, g_lastPoll = 0;

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

// "Melee reach, 3 steps, automatic"; empty while the reach is unknown.
std::string ReachText() {
	if (!g_style || !g_reach) return {};
	const auto& strings = Strings::Current();
	char reach[160]{};
	std::snprintf(reach, sizeof reach, *g_style == AttackReach::Style::Melee ? strings.attackReachMelee : strings.attackReachRanged,
		EntityRadar::DistancePhrase(*g_reach, {}, strings).c_str());
	std::string result = reach;
	result += ", "; result += g_reachSettings.mode == AttackReach::Mode::Auto ? strings.attackModeAuto : strings.attackModeFixed;
	return result;
}

std::string StatusText() {
	const auto& strings = Strings::Current();
	std::string result = g_enabled ? strings.entityCuesEnabled : strings.entityCuesDisabled;
	result += ". "; result += g_settings.npcs ? strings.entityCueNpcsEnabled : strings.entityCueNpcsDisabled;
	result += ". "; result += g_settings.enemies ? strings.entityCueEnemiesEnabled : strings.entityCueEnemiesDisabled;
	result += ". "; result += g_settings.ranged ? strings.entityCueRangedEnabled : strings.entityCueRangedDisabled;
	result += ". "; result += g_settings.ordinary ? strings.entityCueOrdinaryEnabled : strings.entityCueOrdinaryDisabled;
	result += ". "; result += g_reachCue ? strings.entityCueReachEnabled : strings.entityCueReachDisabled;
	if (const auto reach = ReachText(); !reach.empty()) { result += ". "; result += reach; }
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
	case Command::OrdinaryOn: case Command::OrdinaryOff: return g_settings.ordinary ? strings.entityCueOrdinaryEnabled : strings.entityCueOrdinaryDisabled;
	case Command::ReachOn: case Command::ReachOff: return g_reachCue ? strings.entityCueReachEnabled : strings.entityCueReachDisabled;
	case Command::AttackAuto: case Command::AttackMelee: case Command::AttackRanged:
		if (auto reach = ReachText(); !reach.empty()) return reach;
		return command == Command::AttackAuto ? strings.attackModeAuto : strings.attackModeFixed;
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
	g_settings.onScreen = config.onScreen.value_or(g_settings.onScreen);
	g_settings.maxVoices = config.maxVoices.value_or(g_settings.maxVoices);
	g_settings.ordinary = config.ordinary.value_or(g_settings.ordinary);
	g_reachCue = config.reach.value_or(g_reachCue);
	g_reachSettings.mode = config.attack.value_or(g_reachSettings.mode);
	g_reachSettings.meleeReach = config.meleeReach.value_or(g_reachSettings.meleeReach);
	g_reachSettings.rangedReach = config.rangedReach.value_or(g_reachSettings.rangedReach);
	(void)context->RegisterConsoleCommand("access-entity-cues", Console,
		"Entity cues: on|off|status|npc-on|npc-off|enemy-on|enemy-off|ranged-on|ranged-off|ordinary-on|ordinary-off|reach-on|reach-off|attack-auto|attack-melee|attack-ranged.");
	g_audioStarted = EntityCueAudio::Start(context, static_cast<double>(config.volume.value_or(100)) / 100.0,
		RadarConfig::StrengthScale(config.panStrength), RadarConfig::StrengthScale(config.pitchStrength));
	AsyncLog::InfoF(context, "[entity-cues] registered enabled=%u native=%u audio=%u range=%.1f on_screen=%u voices=%zu npc=%u enemy=%u ranged=%u",
		g_enabled, g_supported, g_audioStarted && EntityCueAudio::Available(), g_settings.range,
		g_settings.onScreen, g_settings.maxVoices, g_settings.npcs, g_settings.enemies, g_settings.ranged);
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
		g_tracker.Reset(); { std::lock_guard lock(g_alertMutex); g_alerts.Reset(); }
		Suspend(context, "lifecycle changed");
		g_lastPoll = g_lastLoadout = 0; g_reachTracker.Reset(); g_style.reset(); g_reach.reset();
	}
	const uint64_t now = GetTickCount64();
	if (!state.ready || !g_supported) { g_style.reset(); g_reach.reset(); }
	else if (!g_lastLoadout || now - g_lastLoadout >= LoadoutMs) { g_lastLoadout = now; RefreshReach(context); }
	if (command != Command::None) {
		ScreenReader::Speak(changed ? ChangeText(command) : StatusText(), true);
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
	// The moment a sighted player sees a "!" appear over an NPC (a tester, 2026-10-13); the
	// announcer reads the same tracker, so a mark rings once whoever sees it first.
	if (const auto raised = RaisedAlerts(world, entities); !raised.empty() && g_npcAlerts.load()) {
		RingAlert(raised.front());
		return;
	}
	const auto cue = g_tracker.Update(world, entities, now, g_settings);
	if (!cue) return;
	EntityCueAudio::Sound sound = EntityCueAudio::Sound::Npc;
	switch (cue->kind) {
	case EntityCues::Kind::Npc: sound = EntityCueAudio::Sound::Npc; break;
	case EntityCues::Kind::Important: sound = cue->boss ? EntityCueAudio::Sound::Boss : EntityCueAudio::Sound::Important; break;
	case EntityCues::Kind::Resurrector: sound = EntityCueAudio::Sound::Resurrector; break;
	case EntityCues::Kind::Ranged: sound = EntityCueAudio::Sound::Ranged; break;
	case EntityCues::Kind::Ordinary: sound = EntityCueAudio::Sound::Ordinary; break;
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
	g_reachTracker.Reset(); g_reachSettings = {}; g_style.reset(); g_reach.reset(); g_reachLogged.clear(); g_reachCue = true; g_lastLoadout = 0;
	g_settings = {}; g_enabled = true; g_supported = g_audioStarted = false; g_suspended = true;
	g_revision = g_lastPoll = 0;
}

std::optional<double> CurrentReach() { return g_reach; }
std::optional<double> NamingReach() {
	return g_style == AttackReach::Style::Melee ? g_reach : std::nullopt;
}

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled(Cue cue) {
	return cue == Cue::All ? g_enabled : cue == Cue::Npcs ? g_settings.npcs
		: cue == Cue::Enemies ? g_settings.enemies : cue == Cue::Ordinary ? g_settings.ordinary : cue == Cue::Reach ? g_reachCue : g_settings.ranged;
}
void SetEnabled(Cue cue, bool enabled) {
	g_command = cue == Cue::All ? (enabled ? Command::On : Command::Off)
		: cue == Cue::Npcs ? (enabled ? Command::NpcOn : Command::NpcOff)
		: cue == Cue::Enemies ? (enabled ? Command::EnemyOn : Command::EnemyOff)
		: cue == Cue::Ordinary ? (enabled ? Command::OrdinaryOn : Command::OrdinaryOff)
		: cue == Cue::Reach ? (enabled ? Command::ReachOn : Command::ReachOff)
		: (enabled ? Command::RangedOn : Command::RangedOff);
}
bool NpcAlerts() { return g_npcAlerts.load(); }
void SetNpcAlerts(bool on) { g_npcAlerts = on; }
void PlayNpcAlert() {
	if (g_npcAlerts.load() && g_enabled) EntityCueAudio::Play(EntityCueAudio::Sound::NpcAlert, {});
}
std::vector<EntityCues::Cue> RaisedAlerts(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities) {
	std::lock_guard lock(g_alertMutex);
	return g_alerts.Raised(world, entities);
}
void RingAlert(const EntityCues::Cue& cue) {
	if (!g_npcAlerts.load() || !g_enabled) return;
	AsyncLog::InfoF(g_context, "[npc] exclamation mark id=%u distance=%.1f", cue.id, cue.distance);
	EntityCueAudio::Play(EntityCueAudio::Sound::NpcAlert, {cue.right, cue.front});
}
}
