#include "entity_radar_service.h"
#include "entity_radar.h"
#include "async_log.h"
#include "input_direction.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_world.h"
#include "radar_trace.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <atomic>
#include <charconv>
#include <mutex>
#include <string_view>

namespace D2RAccess::EntityRadarService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, On, Off, Toggle, Status, Up, Left, Right, Down, Range };
struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = 0, revision = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleServiceV1* g_lifecycle = nullptr;
const D2RL::InputServiceV1* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
std::atomic<double> g_requestedRange{0};
EntityRadar::Settings g_settings;
EntityRadar::Cycler g_cycler;
bool g_supported = false, g_units = false, g_enabled = true;
int g_revision = 0;

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
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	struct Word { std::string_view text; Command command; };
	for (const auto& word : {Word{"on", Command::On}, Word{"off", Command::Off}, Word{"status", Command::Status},
		Word{"up", Command::Up}, Word{"left", Command::Left}, Word{"right", Command::Right}, Word{"down", Command::Down}})
		if (arg == word.text) { g_command = word.command; return D2RL::ConsoleCommandResult::Handled; }
	constexpr std::string_view RangeWord = "range ";
	if (arg.starts_with(RangeWord)) {
		double range = 0;
		const auto value = arg.substr(RangeWord.size());
		if (std::from_chars(value.data(), value.data() + value.size(), range).ec == std::errc{} && range > 0) {
			g_requestedRange = range;
			g_command = Command::Range;
			return D2RL::ConsoleCommandResult::Handled;
		}
	}
	return D2RL::ConsoleCommandResult::InvalidArguments;
}
auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* data) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed)
		return D2RL::Input::ActionResult::Ignored;
	g_command = static_cast<Command>(reinterpret_cast<uintptr_t>(data));
	return D2RL::Input::ActionResult::Handled;
}
bool Direction(Command command, Radar::Direction& direction) {
	switch (command) {
	case Command::Up: direction = Radar::Direction::Up; return true;
	case Command::Left: direction = Radar::Direction::Left; return true;
	case Command::Right: direction = Radar::Direction::Right; return true;
	case Command::Down: direction = Radar::Direction::Down; return true;
	default: return false;
	}
}
bool Direction(InputDirection::StickPush push, Radar::Direction& direction) {
	switch (push) {
	case InputDirection::StickPush::Up: direction = Radar::Direction::Up; return true;
	case InputDirection::StickPush::Left: direction = Radar::Direction::Left; return true;
	case InputDirection::StickPush::Right: direction = Radar::Direction::Right; return true;
	case InputDirection::StickPush::Down: direction = Radar::Direction::Down; return true;
	default: return false;
	}
}
// Monsters, NPCs and world objects from the verified unit reader, plus the
// ground items the proximity service already reads.
bool Collect(const Lifecycle& state, std::vector<EntityRadar::Entity>& entities) {
	if (g_units && !NativeEntities::Read(state.player, state.session, entities)) return false;
	GroundItems::Snapshot snapshot;
	if (!NativeGroundItems::Read(state.player, state.session, snapshot)) return false;
	for (const auto& item : snapshot.items) {
		if (item.name.empty()) continue;
		entities.push_back({item.id, EntityRadar::Kind::Item, {item.position.x, item.position.y}, item.name});
	}
	return true;
}
void Scan(const D2RL::PluginContext* context, const Lifecycle& state, Radar::Direction direction, uint64_t now) {
	const auto& strings = Strings::Current();
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	if (!g_supported || !state.ready || RadarUi::Inspect(process == GetCurrentProcessId()) != RadarUi::State::Playing) return;
	Radar::World world;
	std::vector<EntityRadar::Entity> entities;
	if (!NativeWorld::Read(state.player, state.session, state.level, world) || !Collect(state, entities)) {
		AsyncLog::Info(context, "[scan] world unavailable");
		ScreenReader::Speak(strings.scanUnavailable, true);
		return;
	}
	const auto targets = EntityRadar::Scan(world, entities, direction, g_settings);
	const auto result = g_cycler.Push(targets, direction, now, g_settings);
	const auto text = EntityRadar::Describe(result, g_settings, strings);
	AsyncLog::InfoF(context, "[scan] direction=%s position=%.0f,%.0f entities=%zu visible=%zu index=%zu id=%u kind=%u at=%.0f,%.0f distance=%.1f text=%s",
		RadarTrace::DirectionName(static_cast<size_t>(direction)), world.player.x, world.player.y, entities.size(), targets.size(),
		result.index, result.target.entity.id, static_cast<unsigned>(result.target.entity.kind), result.target.entity.position.x,
		result.target.entity.position.y, result.target.distance, text.c_str());
	for (const auto& target : targets)
		AsyncLog::InfoF(context, "[scan]   candidate id=%u kind=%u at=%.0f,%.0f distance=%.1f name=%s", target.entity.id,
			static_cast<unsigned>(target.entity.kind), target.entity.position.x, target.entity.position.y, target.distance, target.entity.name.c_str());
	ScreenReader::Speak(text, true);
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_supported = NativeWorld::Initialize(context) && NativeGroundItems::Initialize(context);
	g_units = g_supported && NativeEntities::Initialize(context);
	if (context->QueryService(D2RL::ServiceId::Lifecycle, D2RL::LifecycleServiceV1Version, &g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceV1Field(g_lifecycle, D2RL::LifecycleServiceV1RequiredSize)) return false;
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged, Kind::PlayerResurrected}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	if (context->QueryService(D2RL::ServiceId::Input, D2RL::InputServiceV1Version, &g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceV1Field(g_input, D2RL::InputServiceV1RequiredSize)) {
		// Keyboard equivalents of the right stick, for testing without a controller.
		struct Shortcut { const char* id; const char* label; D2RL::Input::Key key; Command command; };
		for (const auto& shortcut : {Shortcut{"scan-toggle", "Toggle entity scan", D2RL::Input::Key::E, Command::Toggle},
			Shortcut{"scan-up", "Scan ahead", D2RL::Input::Key::Numpad8, Command::Up},
			Shortcut{"scan-left", "Scan left", D2RL::Input::Key::Numpad4, Command::Left},
			Shortcut{"scan-right", "Scan right", D2RL::Input::Key::Numpad6, Command::Right},
			Shortcut{"scan-down", "Scan behind", D2RL::Input::Key::Numpad2, Command::Down}}) {
			const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize, .logicalId = shortcut.id,
				.displayName = shortcut.label, .category = "Accessibility", .defaultPrimary = {shortcut.key, D2RL::Input::Modifier::Control},
				.callback = Action, .userData = reinterpret_cast<void*>(shortcut.command)};
			D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
			if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
		}
	}
	(void)context->RegisterConsoleCommand("access-scan", Console,
		"Entity scan: on|off|status|up|left|right|down|range <subtiles>. Right stick, or Ctrl+Numpad 8/4/6/2; Ctrl+E toggles.");
	return true;
}

void Tick(const D2RL::PluginContext* context) {
	const auto command = g_command.exchange(Command::None);
	const auto push = InputDirection::TakeRightStickPush();
	if (command == Command::None && push == InputDirection::StickPush::None) return;
	const auto& strings = Strings::Current();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	if (state.revision != g_revision) { g_revision = state.revision; g_cycler.Reset(); }
	if (command == Command::On || command == Command::Off || command == Command::Toggle) {
		g_enabled = command == Command::On || (command == Command::Toggle && !g_enabled);
		g_cycler.Reset();
		ScreenReader::Speak(g_enabled ? strings.scanEnabled : strings.scanDisabled, true);
	}
	if (command == Command::Range) {
		g_settings.range = g_requestedRange;
		AsyncLog::InfoF(context, "[scan] range=%.1f subtiles", g_settings.range);
	}
	if (command == Command::Status) {
		ScreenReader::Speak(g_enabled ? strings.scanEnabled : strings.scanDisabled, true);
		AsyncLog::InfoF(context, "[scan] status supported=%u units=%u ready=%u enabled=%u level=%d range=%.1f metersPerSubtile=%.2f",
			g_supported, g_units, state.ready, g_enabled, state.level, g_settings.range, g_settings.metersPerSubtile);
	}
	Radar::Direction direction = Radar::Direction::Up;
	if (g_enabled && (Direction(command, direction) || Direction(push, direction))) Scan(context, state, direction, GetTickCount64());
}

void Unregister() {
	if (g_lifecycle && g_context)
		for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	if (g_input && g_context)
		for (auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_listeners.clear(); g_actions.clear();
	g_cycler.Reset();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
	g_supported = g_units = false; g_enabled = true;
	g_revision = 0; g_command = Command::None;
	g_settings = {};
}
}
