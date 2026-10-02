#include "entity_radar_service.h"
#include "entity_radar.h"
#include "async_log.h"
#include "native_entities.h"
#include "native_world.h"
#include "npc_proximity.h"
#include "radar_config.h"
#include "radar_trace.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <atomic>
#include <charconv>
#include <mutex>
#include <initializer_list>
#include <string_view>

namespace D2RAccess::EntityRadarService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, NpcOn, NpcOff, NpcToggle, NpcStatus, NpcRange };
struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = 0, revision = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
std::atomic<double> g_requestedRange{0};
EntityRadar::Settings g_settings;
NpcProximity::Settings g_npcSettings;
NpcProximity::Tracker g_npcTracker;
TargetSight::Settings g_sightSettings;
bool g_supported = false, g_units = false, g_npcEnabled = true;
int g_revision = 0;
uint64_t g_npcNextPoll = 0;
// One world + unit read serves every label asked about in the same handful of
// frames. The player moves at most about one subtile in that window, which no
// line of sight turns on.
constexpr uint64_t SightCacheMs = 120;
struct SightCache {
	uint64_t taken = 0;
	int revision = -1;
	bool valid = false;
	Radar::World world;
	std::vector<EntityRadar::Entity> entities;
};
SightCache g_sight;

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) g_state.level = event->currentValue;
	++g_state.revision;
}
struct Word { std::string_view text; Command command; };
// A bare word, or "range <subtiles>".
auto Console(const D2RL::ConsoleCommandContext* command, std::initializer_list<Word> words, Command range) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	for (const auto& word : words)
		if (arg == word.text) { g_command = word.command; return D2RL::ConsoleCommandResult::Handled; }
	constexpr std::string_view RangeWord = "range ";
	if (arg.starts_with(RangeWord)) {
		double value = 0;
		const auto text = arg.substr(RangeWord.size());
		if (std::from_chars(text.data(), text.data() + text.size(), value).ec == std::errc{} && value > 0) {
			g_requestedRange = value;
			g_command = range;
			return D2RL::ConsoleCommandResult::Handled;
		}
	}
	return D2RL::ConsoleCommandResult::InvalidArguments;
}
auto NpcConsole(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	return Console(command, {Word{"on", Command::NpcOn}, Word{"off", Command::NpcOff}, Word{"status", Command::NpcStatus}}, Command::NpcRange);
}
auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* data) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed)
		return D2RL::Input::ActionResult::Ignored;
	g_command = static_cast<Command>(reinterpret_cast<uintptr_t>(data));
	return D2RL::Input::ActionResult::Handled;
}
// Only while the local player is in the world with no panel, dialogue or pause open.
bool Playing(const Lifecycle& state) {
	return g_supported && state.ready && RadarUi::InPlay();
}
// Periodic friendly-NPC poll: names the nearest talk-to NPC once it is in
// range, without interrupting whatever is being read. Silent whenever the UI
// owns speech or the player is not simply playing; the tracker keeps its
// memory across those gaps so nothing repeats when they end.
void Proximity(const D2RL::PluginContext* context, const Lifecycle& state, uint64_t now, bool uiOwnsSpeech) {
	if (!g_npcEnabled || !g_units || uiOwnsSpeech || now < g_npcNextPoll) return;
	g_npcNextPoll = now + g_npcSettings.intervalMs;
	if (!Playing(state)) return;
	Radar::World world;
	std::vector<EntityRadar::Entity> entities;
	if (!NativeWorld::Read(state.player, state.session, state.level, world) || !NativeEntities::Read(state.player, state.session, entities)) return;
	const auto announcement = g_npcTracker.Update(world, entities, g_npcSettings);
	if (!announcement) return;
	const auto text = NpcProximity::Describe(*announcement, g_settings, Strings::Current());
	AsyncLog::InfoF(context, "[npc] position=%.0f,%.0f id=%u at=%.0f,%.0f distance=%.1f direction=%s text=%s", world.player.x, world.player.y,
		announcement->entity.id, announcement->entity.position.x, announcement->entity.position.y, announcement->distance,
		RadarTrace::DirectionName(static_cast<size_t>(announcement->direction)), text.c_str());
	ScreenReader::Speak(text, false);
}
}

TargetSight::Sight LabelSight(std::string_view label) { return LabelTarget(label).sight; }

TargetSight::Bearer LabelTarget(std::string_view label) {
	if (!g_sightSettings.enabled || !g_units || label.empty()) return {};
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	if (!state.ready) return {};
	const auto now = GetTickCount64();
	// A failed read is cached like a good one: without that, `valid` being false
	// bypassed the interval and every UI poll re-copied the collision rooms.
	if (state.revision != g_sight.revision || now - g_sight.taken >= SightCacheMs) {
		g_sight.taken = now;
		g_sight.revision = state.revision;
		g_sight.entities.clear();
		g_sight.valid = NativeWorld::Read(state.player, state.session, state.level, g_sight.world)
			&& NativeEntities::Read(state.player, state.session, g_sight.entities);
	}
	if (!g_sight.valid) return {};
	return TargetSight::Find(g_sight.world, g_sight.entities, label, g_sightSettings);
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_supported = NativeWorld::Initialize(context);
	g_units = g_supported && NativeEntities::Initialize(context);
	const auto npc = RadarConfig::ReadNpc(context);
	g_npcEnabled = npc.enabled.value_or(g_npcEnabled);
	g_npcSettings.range = npc.range.value_or(g_npcSettings.range);
	g_npcSettings.requireLineOfSight = npc.lineOfSight.value_or(g_npcSettings.requireLineOfSight);
	AsyncLog::InfoF(context, "[npc] enabled=%u range=%.1f lineOfSight=%u units=%u", g_npcEnabled, g_npcSettings.range, g_npcSettings.requireLineOfSight, g_units);
	const auto target = RadarConfig::ReadTarget(context);
	g_sightSettings.enabled = target.lineOfSight.value_or(g_sightSettings.enabled);
	g_sightSettings.reach = target.reach.value_or(g_sightSettings.reach);
	AsyncLog::InfoF(context, "[target] lineOfSight=%u reach=%.1f", g_sightSettings.enabled, g_sightSettings.reach);
	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged, Kind::PlayerResurrected}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize, .logicalId = "npc-toggle",
			.displayName = "Toggle NPC announcer", .category = "Accessibility", .defaultPrimary = {D2RL::Input::Key::N, D2RL::Input::Modifier::Control},
			.callback = Action, .userData = reinterpret_cast<void*>(Command::NpcToggle)};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
	}
	(void)context->RegisterConsoleCommand("access-npc", NpcConsole,
		"Friendly NPC announcer: on|off|status|range <subtiles>. Names the nearest NPC in range while playing; Ctrl+N toggles.");
	return true;
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	const auto command = g_command.exchange(Command::None);
	const auto now = GetTickCount64();
	const auto& strings = Strings::Current();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	if (state.revision != g_revision) { g_revision = state.revision; g_npcTracker.Reset(); }
	if (command == Command::NpcOn || command == Command::NpcOff || command == Command::NpcToggle) {
		g_npcEnabled = command == Command::NpcOn || (command == Command::NpcToggle && !g_npcEnabled);
		g_npcTracker.Reset();
		ScreenReader::Speak(g_npcEnabled ? strings.npcEnabled : strings.npcDisabled, true);
	}
	if (command == Command::NpcRange) {
		g_npcSettings.range = g_requestedRange;
		AsyncLog::InfoF(context, "[npc] range=%.1f subtiles", g_npcSettings.range);
	}
	if (command == Command::NpcStatus) {
		ScreenReader::Speak(g_npcEnabled ? strings.npcEnabled : strings.npcDisabled, true);
		AsyncLog::InfoF(context, "[npc] status enabled=%u units=%u ready=%u range=%.1f lineOfSight=%u", g_npcEnabled, g_units, state.ready,
			g_npcSettings.range, g_npcSettings.requireLineOfSight);
	}
	Proximity(context, state, now, uiOwnsSpeech);
}

void Unregister() {
	if (g_lifecycle && g_context)
		for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	if (g_input && g_context)
		for (auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_listeners.clear(); g_actions.clear();
	g_npcTracker.Reset();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
	g_supported = g_units = false; g_npcEnabled = true;
	g_revision = 0; g_command = Command::None; g_npcNextPoll = 0;
	g_settings = {}; g_npcSettings = {}; g_sightSettings = {}; g_sight = {};
}
}
