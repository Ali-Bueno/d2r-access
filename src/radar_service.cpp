#include "radar_service.h"
#include "async_log.h"
#include "native_entities.h"
#include "native_world.h"
#include "native_ui.h"
#include "room_service.h"
#include "terrain_service.h"
#include "native_widgets.h"
#include "radar_audio.h"
#include "input_direction.h"
#include "radar_config.h"
#include "radar_readout.h"
#include "radar_trace.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "wall_slide_service.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace D2RAccess::RadarService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, On, Off, Toggle, Status, Dump, Test };
constexpr uint64_t TestStepMs = 400; // audition spacing: well over one cue plus its gap
constexpr int DumpRadius = 12; // 25x25 local ASCII diagnostic, not sensor range
// Collision snapshots are bulky records; one per second keeps AsyncLog's
// bounded queue for the cue events themselves.
constexpr uint64_t SnapshotIntervalMs = 1000;
// Level exits do not move; rooms stream in at walking pace, so a second is enough.
constexpr uint64_t ExitsRefreshMs = 1000;
// The audition plays every reading in every direction, the marked exit and
// floor (unexplored tail) right after their plain form.
struct AuditionStep { Radar::State state; bool unexplored; };
constexpr AuditionStep Audition[] = {{Radar::State::Exit, false}, {Radar::State::Exit, true}, {Radar::State::Open, false},
	{Radar::State::Open, true}, {Radar::State::Blocked, false}, {Radar::State::Door, false}};
constexpr int AuditionSteps = static_cast<int>(std::size(Audition) * Radar::DirectionCount);
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
Radar::Engine g_engine;
bool g_supported = false, g_ready = false, g_enabled = true, g_suspended = true;
uint32_t g_playerId = 0;
uint64_t g_session = 0, g_lastPoll = 0, g_testDue = 0, g_lastSnapshot = 0;
int g_level = 0, g_testStep = -1;
std::string g_lastReason;
std::vector<Radar::LevelExit> g_exits;
uint64_t g_exitsAt = 0;
int g_exitsLevel = 0;

// The level's exits for the passage scan; a failed read keeps the last list.
void RefreshExits(const Radar::World& world, uint64_t now) {
	if (g_exitsLevel == world.level && g_exitsAt && now - g_exitsAt < ExitsRefreshMs) return;
	std::vector<EntityRadar::Entity> entities;
	// Include area borders; the passage scan still requires local walkable reach.
	if (!NativeEntities::Read(g_playerId, world.session, entities, true, true)) return;
	g_exits.clear();
	for (const auto& entity : entities)
		if (entity.kind == EntityRadar::Kind::Exit) g_exits.push_back({entity.position, entity.destinationLevel});
	g_exitsAt = now;
	g_exitsLevel = world.level;
}

void Suspend(const char* reason = "suspended") {
	if (!g_suspended || g_testStep >= 0) RadarAudio::Silence(reason);
	g_suspended = true;
	g_testStep = -1;
	g_engine.Reset();
}
void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	Suspend("lifecycle");
	g_session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_ready = false; g_level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_ready = true; g_playerId = event->playerId; g_level = 0; }
	else if (event->kind == Kind::ActChanged) g_level = 0;
	else if (event->kind == Kind::LevelChanged) g_level = event->currentValue;
	AsyncLog::InfoF(g_context, "[radar] lifecycle=%u session=%llu player=%u ready=%u level=%d previous=%d current=%d",
		static_cast<unsigned>(event->kind), static_cast<unsigned long long>(g_session), g_playerId, g_ready ? 1u : 0u,
		g_level, event->previousValue, event->currentValue);
}

RadarUi::State UiState() {
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return RadarUi::Inspect(process == GetCurrentProcessId());
}

void Describe(bool available, RadarUi::State ui) {
	const auto& text = Strings::Current();
	if (!g_enabled) { ScreenReader::Speak(text.radarDisabled, true); return; }
	if (!RadarAudio::Available()) { ScreenReader::Speak(text.radarAudioUnavailable, true); return; }
	if (!g_supported) { ScreenReader::Speak(text.radarReaderUnavailable, true); return; }
	if (!g_ready) { ScreenReader::Speak(text.radarWaitingPlayer, true); return; }
	if (ui != RadarUi::State::Playing) { ScreenReader::Speak(text.radarUiStates[static_cast<size_t>(ui) - 1], true); return; }
	if (!available) { ScreenReader::Speak(text.radarUnavailable, true); return; }
	ScreenReader::Speak(RadarReadout::Describe(g_engine, text), true);
}
void LogSensors(uint64_t now) {
	const auto velocity = g_engine.Velocity();
	AsyncLog::InfoF(g_context, "[radar-motion] velocity=%.1f,%.1f speed=%.1f way_through=%.1f", velocity.x, velocity.y, g_engine.Speed(), g_engine.CurrentWayThrough());
	for (size_t i = 0; i < Radar::DirectionCount; ++i) {
		const auto& s = g_engine.Sensors()[i];
		AsyncLog::InfoF(g_context, "[radar-state] direction=%s state=%s unexplored=%u candidate=%s candidate_age=%llu depth=%.1f open=%.1f close=%.1f available=%u observed=%s deferred=%s last_cue=%s last_cue_age=%llu hold=%s frontier=%u,%.1f",
			RadarTrace::DirectionName(i), RadarTrace::StateName(s.state), s.unexplored ? 1u : 0u, RadarTrace::StateName(s.candidate),
			static_cast<unsigned long long>(s.candidate == Radar::State::Unknown ? 0 : now - s.candidateSince),
			s.passage.depth, s.passage.openDepth, s.passage.closeDepth, s.passage.available ? 1u : 0u, RadarTrace::StateName(s.passage.observed),
			RadarTrace::StateName(s.deferred), RadarTrace::StateName(s.lastCue),
			static_cast<unsigned long long>(s.lastCueMs ? now - s.lastCueMs : 0), s.hold, s.frontier.known ? 1u : 0u, s.frontier.projection);
	}
}
void Dump(const Radar::World& world) {
	AsyncLog::InfoF(g_context, "[radar-map] session=%llu level=%d position=%.0f,%.0f rooms=%zu pattern=%d mask=%04X axes=screen; # blocked, A occupied, . open, ? unknown, @ player; rows +Y, columns +X",
		static_cast<unsigned long long>(world.session), world.level, world.player.x, world.player.y,
		world.rooms.size(), world.bodyPattern, world.walkingMask);
	AsyncLog::InfoF(g_context, "[radar-motion] position=%.5f,%.5f source=path-16.16",
		world.player.x + world.subtileFraction.x, world.player.y + world.subtileFraction.y);
	for (int y = -DumpRadius; y <= DumpRadius; ++y) {
		std::string line;
		for (int x = -DumpRadius; x <= DumpRadius; ++x) {
			const auto cell = world.Cell(static_cast<int>(world.player.x) + x, static_cast<int>(world.player.y) + y);
			const auto terrain = world.Cell(static_cast<int>(world.player.x) + x, static_cast<int>(world.player.y) + y, Radar::Layer::Terrain);
			line += x == 0 && y == 0 ? '@' : cell == Radar::State::Open ? '.' : cell == Radar::State::Blocked ? (terrain == Radar::State::Open ? 'A' : '#') : '?';
		}
		AsyncLog::InfoF(g_context, "[radar-map] %s", line.c_str());
	}
}
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	Command request = Command::None;
	if (arg == "on") request = Command::On;
	else if (arg == "off") request = Command::Off;
	else if (arg == "status") request = Command::Status;
	else if (arg == "dump") request = Command::Dump;
	else if (arg == "test") request = Command::Test;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	g_command = request;
	return D2RL::ConsoleCommandResult::Handled;
}
auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* data) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed) return D2RL::Input::ActionResult::Ignored;
	g_command = static_cast<Command>(reinterpret_cast<uintptr_t>(data));
	return D2RL::Input::ActionResult::Handled;
}
void LogGenerated(const Radar::World& world, const Radar::Events& events, const Radar::Marks& marks, uint64_t now) {
	for (size_t i = 0; i < events.size(); ++i) {
		if (events[i] == Radar::State::Unknown) continue;
		const auto& sensor = g_engine.Sensors()[i];
		const auto velocity = g_engine.Velocity();
		const auto stick = InputDirection::LeftStick();
		AsyncLog::InfoF(g_context, "[radar] direction=%zu state=%u unexplored=%u position=%.0f,%.0f axes=screen depth=%.1f open=%.1f close=%.1f speed=%.1f event_ms=%llu stage=generated direction_name=%s state_name=%s session=%llu level=%d velocity=%.1f,%.1f stick=%.2f,%.2f trigger=passage",
			i, static_cast<unsigned>(events[i]), marks[i] ? 1u : 0u, world.player.x, world.player.y, sensor.passage.depth, sensor.passage.openDepth,
			sensor.passage.closeDepth, g_engine.Speed(), static_cast<unsigned long long>(now), RadarTrace::DirectionName(i),
			RadarTrace::StateName(events[i]), static_cast<unsigned long long>(world.session), world.level, velocity.x, velocity.y,
			stick.connected ? stick.x : 0.0f, stick.connected ? stick.y : 0.0f);
	}
}
}

bool Register(const D2RL::PluginContext* context) {
	g_context = context;
	const auto config = RadarConfig::Read(context);
	g_enabled = config.enabled;
	g_supported = NativeWorld::Initialize(context);
	if (!context || context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) { Unregister(); return false; }
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged, Kind::PlayerResurrected}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		struct Shortcut { const char* id; const char* label; D2RL::Input::Key key; Command command; };
		const Shortcut shortcuts[] = {{"radar-toggle", "Toggle navigation radar", D2RL::Input::Key::R, Command::Toggle},
			{"radar-status", "Read navigation radar", D2RL::Input::Key::T, Command::Status}};
		for (const auto& shortcut : shortcuts) {
			const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = shortcut.id, .displayName = shortcut.label, .category = "Accessibility",
				.defaultPrimary = {shortcut.key, D2RL::Input::Modifier::Control}, .callback = Action,
				.userData = reinterpret_cast<void*>(shortcut.command)};
			D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
			if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
		}
	}
	(void)context->RegisterConsoleCommand("access-radar", Console, "Navigation sonar: on|off|status|dump|test.");
	(void)RadarAudio::Start(context, static_cast<double>(config.volume) / RadarConfig::MaxVolume);
	AsyncLog::InfoF(context, "[radar] Four-direction sonar registered (volume %u%%); waiting for local player and level.", config.volume);
	return true;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return; // Register failed or Unregister ran
	const auto command = g_command.exchange(Command::None);
	const uint64_t now = GetTickCount64();
	if (command == Command::On || command == Command::Off || command == Command::Toggle) {
		g_enabled = command == Command::On || (command == Command::Toggle && !g_enabled);
		Suspend(g_enabled ? "enabled-reset" : "disabled");
		ScreenReader::Speak(g_enabled ? Strings::Current().radarEnabled : Strings::Current().radarDisabled, true);
	}
	if (command == Command::Test && RadarAudio::Available()) {
		Suspend("audition");
		g_testStep = 0;
		g_testDue = now + TestStepMs * 3; // let the introductory speech begin first
		ScreenReader::Speak(Strings::Current().radarTest, true);
	}
	if (command == Command::None && now - g_lastPoll < Radar::PollMs) return;
	g_lastPoll = now;
	const bool gate = g_supported && g_ready && g_enabled;
	// The widget-tree walk is only worth its cost while the radar can run.
	const auto ui = gate || command != Command::None ? UiState() : RadarUi::State::Playing;
	const bool blocked = ui != RadarUi::State::Playing;
	if (g_testStep >= 0 && !blocked) {
		if (now >= g_testDue) {
			if (g_testStep == AuditionSteps) { g_testStep = -1; return; }
			Radar::Events events{};
			Radar::Marks marks{};
			const size_t step = static_cast<size_t>(g_testStep++);
			events[step % Radar::DirectionCount] = Audition[step / Radar::DirectionCount].state;
			marks[step % Radar::DirectionCount] = Audition[step / Radar::DirectionCount].unexplored;
			RadarAudio::Play(events, {.eventMs = now, .session = g_session, .level = g_level, .audition = true}, marks);
			g_testDue = now + TestStepMs;
		}
		return;
	}
	Radar::World world;
	NativeWorld::Diagnostics detail;
	const bool available = gate && !blocked && NativeWorld::Read(g_playerId, g_session, g_level, world, &detail);
	constexpr const char* UiReasons[] = {"playing", "inactive-window", "ui-unavailable", "dialogue", "pause"};
	const char* reason = !g_supported ? "reader-unavailable" : !g_enabled ? "disabled" : !g_ready ? "waiting-player"
		: blocked ? UiReasons[static_cast<size_t>(ui)] : detail.stage;
	if (g_lastReason != reason || command == Command::Status || command == Command::Dump) {
		AsyncLog::InfoF(context, "[radar] status=%s expectedPlayer=%u actualPlayer=%u expectedLevel=%d actualLevel=%d position=%d,%d pattern=%d footprint=%04X movement=%04X rawCell=%04X removedPresence=%u presenceOffset=%d,%d axes=screen",
			reason, g_playerId, detail.playerId, g_level, detail.level, detail.x, detail.y, detail.pattern,
			detail.footprintMask, detail.movementMask, static_cast<unsigned>(detail.rawPlayerCell), detail.removedPresenceCells,
			detail.presenceOffsetX, detail.presenceOffsetY);
		g_lastReason = reason;
	}
	if (!available) {
		Suspend(reason);
	} else {
		world.maze = RoomService::Maze(world.level);
		world.slides = !WallSlideService::Enabled();
		RefreshExits(world, now);
		world.exits = g_exits;
		TerrainService::Qualify(world);
		if (g_suspended) AsyncLog::InfoF(context, "[radar] Live terrain ready: level=%d position=%.0f,%.0f rooms=%zu pattern=%d maze=%u slides=%u exits=%zu; silent baseline.", world.level, world.player.x, world.player.y, world.rooms.size(), world.bodyPattern, world.maze ? 1u : 0u, world.slides ? 1u : 0u, world.exits.size());
		g_suspended = false;
		const uint64_t revision = g_engine.Revision();
		Radar::Marks marks{};
		const auto events = g_engine.Tick(world, now, &marks);
		if (revision != g_engine.Revision()) RadarAudio::Silence("world-rebased");
		Radar::Events states{};
		for (size_t i = 0; i < states.size(); ++i) states[i] = g_engine.Sensors()[i].state;
		RadarAudio::Retain(states, g_engine.Unexplored());
		LogGenerated(world, events, marks, now);
		RadarAudio::Play(events, {.eventMs = now, .session = world.session, .level = world.level, .position = world.player}, marks);
		const bool generated = std::any_of(events.begin(), events.end(), [](auto state) { return state != Radar::State::Unknown; });
		if (generated && (!g_lastSnapshot || now - g_lastSnapshot >= SnapshotIntervalMs)) {
			RadarTrace::Snapshot(context, world, g_engine, now);
			g_lastSnapshot = now;
		}
	}
	if (command == Command::Status || command == Command::Dump) {
		Describe(available, ui);
		if (available) { LogSensors(now); Dump(world); }
	}
}

void Unregister() {
	Suspend("unregister");
	RadarAudio::Stop();
	if (g_context && g_lifecycle) for (const auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	if (g_context && g_input) for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_listeners.clear();
	g_actions.clear();
	g_context = nullptr;
	g_lifecycle = nullptr;
	g_input = nullptr;
	g_ready = g_supported = false;
}
}
