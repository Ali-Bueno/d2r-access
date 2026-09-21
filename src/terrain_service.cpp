#include "terrain_service.h"
#include "async_log.h"
#include "entity_radar.h"
#include "explored_rooms.h"
#include "input_direction.h"
#include "native_entities.h"
#include "native_level_exits.h"
#include "native_world.h"
#include "radar.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "terrain_overview.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::TerrainService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;

// Rooms stream in as the player walks, which is slow: five radar polls is the
// same cadence the room announcer uses, and far cheaper than the radar's.
constexpr uint64_t PollMs = 5 * Radar::PollMs;
// Log the explored count again once it has grown by this share of the level.
constexpr size_t LogGrowthPercent = 10;

struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<bool> g_asked{false};
// Distances are spoken by the same rule as the entity scan and the exit review.
const EntityRadar::Settings g_scan;
ExploredRooms::Memory g_explored;
uint64_t g_nextPoll = 0;
uint32_t g_loggedLevel = 0;
size_t g_loggedRooms = 0;
uint32_t g_warnedLevel = 0;
bool g_supported = false;

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	// A new session is a new map: what was walked in the old one is gone.
	const bool newSession = event->sessionGeneration != g_state.session
		|| event->kind == Kind::GameJoined || event->kind == Kind::GameLeft;
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) g_state.level = event->currentValue;
	if (newSession) {
		g_explored.Forget();
		g_loggedLevel = 0;
		g_loggedRooms = 0;
	}
}
bool Playing() {
	return RadarUi::InPlay();
}
std::string LevelName(uint32_t level, const Strings::Table& strings) {
	auto name = NativeEntities::LevelName(level);
	return name.empty() ? strings.exitUnnamed : name;
}
// The level's own rect is the union of its rooms; a disagreement means the
// layout was read while the game was rebuilding it, so say so once.
void CheckBounds(const D2RL::PluginContext* context, const NativeLevelExits::Layout& native,
	const NativeLevelExits::TileRect& box) {
	if (native.level == g_warnedLevel || (box.x == native.rect.x && box.y == native.rect.y
		&& box.width == native.rect.width && box.height == native.rect.height)) return;
	g_warnedLevel = native.level;
	AsyncLog::WarnF(context, "[terrain] level=%u rect=%d,%d %dx%d differs from the room union %d,%d %dx%d",
		native.level, native.rect.x, native.rect.y, native.rect.width, native.rect.height,
		box.x, box.y, box.width, box.height);
}
// One spoken summary of the level around the player.
void Overview(const D2RL::PluginContext* context, const Lifecycle& state) {
	const auto& strings = Strings::Current();
	Radar::World world;
	NativeLevelExits::Layout native;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)
		|| !NativeEntities::ReadLayout(state.player, true, native)) {
		ScreenReader::Speak(strings.terrainUnavailable, true);
		AsyncLog::InfoF(context, "[terrain] layout unavailable level=%d", state.level);
		return;
	}
	TerrainOverview::Layout layout;
	layout.rooms.reserve(native.rooms.size());
	for (const auto& room : native.rooms)
		layout.rooms.push_back({room.rect, g_explored.Explored(native.level, room.rect.x, room.rect.y)});
	for (const auto& neighbour : native.neighbours)
		layout.neighbours.push_back({neighbour.rect, LevelName(neighbour.level, strings)});
	const auto summary = TerrainOverview::Describe(layout, world.player);
	if (!summary.valid) {
		ScreenReader::Speak(strings.terrainUnavailable, true);
		AsyncLog::InfoF(context, "[terrain] off the room grid at=%.0f,%.0f level=%u rooms=%zu",
			world.player.x, world.player.y, native.level, native.rooms.size());
		return;
	}
	CheckBounds(context, native, summary.box);
	const auto text = TerrainOverview::Speak(summary, LevelName(native.level, strings), g_scan, strings);
	ScreenReader::Speak(text, true);
	std::string legs;
	for (const auto& leg : summary.legs) {
		char entry[96]{};
		std::snprintf(entry, sizeof entry, " edge=%.0f unexplored=%.0f border=%s", leg.edge,
			leg.hasUnexplored ? leg.unexplored : 0.0, leg.border.empty() ? "-" : leg.border.c_str());
		legs += entry;
	}
	AsyncLog::InfoF(context, "[terrain] at=%.0f,%.0f level=%u box=%d,%d %dx%d place=%zu rooms=%zu/%zu%s -> %s",
		world.player.x, world.player.y, native.level, summary.box.x, summary.box.y,
		summary.box.width, summary.box.height, summary.place, g_explored.Count(native.level),
		native.rooms.size(), legs.c_str(), text.c_str());
}
// One poll: every room the game keeps streamed has been walked into.
void Track(const D2RL::PluginContext* context, const Lifecycle& state) {
	NativeLevelExits::Layout native;
	if (!NativeEntities::ReadLayout(state.player, false, native)) return;
	for (const auto& room : native.rooms)
		if (room.active) g_explored.Mark(native.level, room.rect.x, room.rect.y);
	const size_t explored = g_explored.Count(native.level);
	const size_t step = std::max<size_t>(1, native.rooms.size() * LogGrowthPercent / 100);
	if (native.level == g_loggedLevel && explored < g_loggedRooms + step) return;
	g_loggedLevel = native.level;
	g_loggedRooms = explored;
	AsyncLog::InfoF(context, "[terrain] explored rooms=%zu/%zu level=%u", explored, native.rooms.size(), native.level);
}
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg.empty() || arg == "read") g_asked = true;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize)
		|| event->kind != D2RL::Input::ActionEventKind::Pressed) return D2RL::Input::ActionResult::Ignored;
	g_asked = true;
	return D2RL::Input::ActionResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_supported = NativeWorld::Initialize(context) && NativeEntities::Initialize(context);
	(void)context->RegisterConsoleCommand("access-terrain", Console,
		"Terrain overview: how far the level reaches and what is unexplored. Ctrl+O, or press both sticks in together.");
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		// Keyboard equivalent of pressing both sticks in together.
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "terrain-overview", .displayName = "Read the terrain overview", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::O, D2RL::Input::Modifier::Control}, .callback = Action};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
	}
	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{
			.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle)
			!= D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	AsyncLog::InfoF(context, "[terrain] supported=%u", g_supported ? 1u : 0u);
	return g_supported;
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	// Both sources are consumed every tick so a latched gesture cannot fire late.
	const bool asked = g_asked.exchange(false);
	const bool clicked = InputDirection::TakeStickChord();
	const bool requested = asked || clicked;
	if (!g_supported) {
		if (requested) ScreenReader::Speak(Strings::Current().terrainUnavailable, true);
		return;
	}
	const uint64_t now = GetTickCount64();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	// An asked-for readout answers even while the UI reader is talking.
	if (requested && state.ready && Playing()) {
		std::lock_guard lock(g_mutex);
		Overview(context, state);
	}
	if (uiOwnsSpeech || now - g_nextPoll < PollMs) return;
	g_nextPoll = now;
	if (!state.ready || !Playing()) return;
	std::lock_guard lock(g_mutex);
	Track(context, state);
}

void Unregister() {
	if (g_lifecycle) for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	g_listeners.clear();
	if (g_input) for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_actions.clear();
	std::lock_guard lock(g_mutex);
	g_explored.Forget();
	g_state = {};
	g_asked = false;
	g_supported = false;
	g_nextPoll = 0;
	g_loggedLevel = 0;
	g_loggedRooms = 0;
	g_warnedLevel = 0;
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
}
}
