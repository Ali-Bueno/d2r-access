#include "explore_service.h"

#include "async_log.h"
#include "explored_map.h"
#include "native_world.h"
#include "navigation_grid.h"
#include "radar_config.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "unexplored_route.h"

#include <windows.h>
#include <atomic>
#include <mutex>
#include <optional>
#include <string_view>

namespace D2RAccess::ExploreService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, Route, Status };
struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = 0, revision = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
Explored::Map g_explored;
Explored::Settings g_exploredSettings;
UnexploredRoute::Settings g_routeSettings;
// The place the last press sent the player to. Holding on to it is what makes
// two presses agree: without it every press picks the nearest unexplored cell
// again, and after a few steps that is a different one.
std::optional<Radar::Position> g_target;
bool g_supported = false, g_enabled = true;
int g_revision = -1;
uint64_t g_nextMark = 0;
// The memory only has to keep up with a running player: nine subtiles a second
// against a twelve-subtile brush leaves no gaps between two marks.
constexpr uint64_t MarkIntervalMs = 250;

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
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg.empty() || arg == "now") { g_command = Command::Route; return D2RL::ConsoleCommandResult::Handled; }
	if (arg == "status") { g_command = Command::Status; return D2RL::ConsoleCommandResult::Handled; }
	return D2RL::ConsoleCommandResult::InvalidArguments;
}

auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed)
		return D2RL::Input::ActionResult::Ignored;
	g_command = Command::Route;
	return D2RL::Input::ActionResult::Handled;
}

// The world read the memory and the route share. The level the lifecycle
// reports leads the world by one event on a transition, so the map follows the
// level the world itself reports.
bool ReadWorld(const Lifecycle& state, Radar::World& world) {
	if (!g_supported || !state.ready) return false;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return false;
	g_explored.Follow(world.session, world.level);
	return true;
}

void Route(const D2RL::PluginContext* context, const Lifecycle& state) {
	const auto& strings = Strings::Current();
	Radar::World world;
	if (!g_enabled || !ReadWorld(state, world)) {
		AsyncLog::InfoF(context, "[explore] unavailable enabled=%u supported=%u ready=%u", g_enabled, g_supported, state.ready);
		ScreenReader::Speak(strings.exploreUnavailable, true);
		return;
	}
	g_explored.Mark(world.player, g_exploredSettings);
	const auto grid = Navigation::Build(world, g_routeSettings.radius);
	const auto route = UnexploredRoute::Find(grid, g_explored, g_routeSettings, {g_target});
	g_target = route.outcome == UnexploredRoute::Outcome::Route ? std::optional{route.target} : std::nullopt;
	const auto text = UnexploredRoute::Describe(route, strings);
	AsyncLog::InfoF(context, "[explore] position=%.0f,%.0f level=%d outcome=%u target=%.0f,%.0f distance=%d runs=%zu truncated=%u kept=%u arrived=%u chunks=%zu text=%s",
		world.player.x, world.player.y, world.level, static_cast<unsigned>(route.outcome), route.target.x, route.target.y,
		route.distance, route.steps.size(), route.truncated, route.keptLocked, route.arrived, g_explored.Chunks(), text.c_str());
	ScreenReader::Speak(text, true);
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_supported = NativeWorld::Initialize(context);
	const auto config = RadarConfig::ReadExplore(context);
	g_enabled = config.enabled.value_or(g_enabled);
	g_routeSettings.radius = static_cast<int>(config.radius.value_or(g_routeSettings.radius));
	g_exploredSettings.markRadius = static_cast<int>(config.markRadius.value_or(g_exploredSettings.markRadius));
	g_routeSettings.probeRadius = static_cast<int>(config.probeRadius.value_or(g_routeSettings.probeRadius));
	AsyncLog::InfoF(context, "[explore] enabled=%u radius=%d markRadius=%d probeRadius=%d supported=%u",
		g_enabled, g_routeSettings.radius, g_exploredSettings.markRadius, g_routeSettings.probeRadius, g_supported);
	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		// The slash key, as in the DevilutionX mod this comes from; rebindable
		// through the loader like every other action.
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "explore-route", .displayName = "Route to the nearest unexplored space", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::Slash, D2RL::Input::Modifier::None}, .callback = Action};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		const bool bound = g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success;
		if (bound) g_actions.push_back(handle);
		// Pressing a key that was never bound looks exactly like a broken
		// feature, so the log says which of the two happened.
		AsyncLog::InfoF(context, "[explore] slash key %s", bound ? "bound" : "not bound; use access-explore");
	}
	(void)context->RegisterConsoleCommand("access-explore",
		Console, "Nearest unexplored space: no argument or 'now' speaks the route, 'status' logs the memory. Slash key.");
	return true;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	const auto now = GetTickCount64();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	// A new level or game invalidates the place the last press was heading for.
	if (state.revision != g_revision) { g_revision = state.revision; g_target.reset(); }
	if (g_enabled && now >= g_nextMark) {
		g_nextMark = now + MarkIntervalMs;
		Radar::World world;
		if (ReadWorld(state, world)) g_explored.Mark(world.player, g_exploredSettings);
	}
	if (command == Command::Route) Route(context, state);
	if (command == Command::Status)
		AsyncLog::InfoF(context, "[explore] status enabled=%u supported=%u ready=%u level=%d chunks=%zu radius=%d markRadius=%d probeRadius=%d target=%.0f,%.0f",
			g_enabled, g_supported, state.ready, g_explored.Level(), g_explored.Chunks(), g_routeSettings.radius,
			g_exploredSettings.markRadius, g_routeSettings.probeRadius,
			g_target ? g_target->x : 0.0, g_target ? g_target->y : 0.0);
}

void Unregister() {
	if (g_lifecycle && g_context)
		for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	if (g_input && g_context)
		for (auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_listeners.clear(); g_actions.clear();
	g_explored.Reset();
	g_target.reset();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
	g_supported = false; g_enabled = true; g_revision = -1; g_nextMark = 0;
	g_command = Command::None;
	g_exploredSettings = {}; g_routeSettings = {};
}
}
