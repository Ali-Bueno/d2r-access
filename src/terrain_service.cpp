#include "terrain_service.h"
#include "async_log.h"
#include "entity_radar.h"
#include "explored_rooms.h"
#include "exploration_cue_service.h"
#include "explored_store.h"
#include "frontier.h"
#include "input_direction.h"
#include "level_exit_layout.h"
#include "native_drlg.h"
#include "native_entities.h"
#include "native_level_exits.h"
#include "native_world.h"
#include "radar.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "room_service.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "terrain_overview.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
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
// The memory reaches the disk this often while it changes, and on every level
// change and shutdown; a crash loses at most this much walking.
constexpr uint64_t SaveIntervalMs = 30000;

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
uint64_t g_nextPoll = 0, g_nextSave = 0;
uint32_t g_loggedLevel = 0;
size_t g_loggedRooms = 0;
uint32_t g_warnedLevel = 0;
bool g_supported = false, g_roomSounds = true;
// The store worker owns archive I/O; gameplay only submits room snapshots.
std::filesystem::path g_directory;
ExploredStore::Store g_store;
std::map<uint32_t, ExploredStore::Key> g_keys;
uint32_t g_loadedLevel = 0;
uint64_t g_nextLoadTry = 0; // the radar asks every poll; the layout is read at most this often
uint32_t g_checkedLevel = 0; // grid-origin cross-check done for this level
// The frontier quadrants of the level the radar is qualified against.
Radar::Marks g_frontier{};
int g_frontierLevel = 0;

// Caller holds g_mutex.
void SaveAll(const D2RL::PluginContext* context) {
	if (!g_explored.Dirty() || g_directory.empty()) return;
	std::vector<ExploredStore::Update> updates;
	for (const auto& [level, key] : g_keys)
		updates.push_back({key, g_explored.Snapshot(level)});
	g_store.Submit(std::move(updates));
	if (g_store.Failed()) AsyncLog::Warn(context, "[terrain] exploration archive unavailable; pending changes will retry.");
}
// Caller holds g_mutex. Loads the level's file once per visit; the seed can
// take a few polls to become readable, so an unavailable seed retries.
void LoadLevel(const D2RL::PluginContext* context, const NativeLevelExits::Layout& native) {
	if (g_loadedLevel == native.level || g_directory.empty()) return;
	g_nextLoadTry = GetTickCount64() + PollMs;
	const uint32_t seed = NativeDrlg::MapSeed(NativeWorld::PlayerRoom(), static_cast<int>(native.level));
	if (!seed) return;
	const ExploredStore::Key key{seed, native.level, ExploredStore::LayoutDigest(native.rooms)};
	g_keys[native.level] = key;
	if (!g_store.Ready()) return; // archive loading and legacy migration never block this tick
	const bool loaded = g_store.Load(key, g_explored);
	g_loadedLevel = native.level;
	AsyncLog::InfoF(context, "[terrain] level=%u seed=%08X rooms=%zu %s %s: seen=%zu visited=%zu", native.level, seed,
		native.rooms.size(), loaded ? "loaded" : "new", ExploredStore::ArchiveFile(g_directory).filename().string().c_str(),
		g_explored.Count(native.level), g_explored.VisitedCount(native.level));
}
void __cdecl OnGameplay(const D2RL::PluginContext* context, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	// A new session is a new map: what was walked in the old one is on disk.
	ExplorationCueService::Silence();
	const bool newSession = event->sessionGeneration != g_state.session
		|| event->kind == Kind::GameJoined || event->kind == Kind::GameLeft;
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) g_state.level = event->currentValue;
	SaveAll(context);
	g_loadedLevel = 0;
	g_nextLoadTry = 0;
	g_checkedLevel = 0;
	g_frontier = {};
	g_frontierLevel = 0;
	if (newSession) {
		g_explored.Forget();
		g_keys.clear();
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
	AsyncLog::InfoF(context, "[terrain] at=%.0f,%.0f level=%u box=%d,%d %dx%d place=%zu rooms=%zu/%zu percent=%d%s -> %s",
		world.player.x, world.player.y, native.level, summary.box.x, summary.box.y,
		summary.box.width, summary.box.height, summary.place, g_explored.Count(native.level),
		native.rooms.size(), summary.percent, legs.c_str(), text.c_str());
}
// The rooms never streamed, as subtile rectangles, for the frontier.
std::vector<NativeRooms::Bounds> Unseen(const NativeLevelExits::Layout& native) {
	std::vector<NativeRooms::Bounds> unseen;
	for (const auto& room : native.rooms) {
		if (g_explored.Explored(native.level, room.rect.x, room.rect.y)) continue;
		unseen.push_back(NativeRooms::Subtiles({room.rect.x, room.rect.y, room.rect.width, room.rect.height}));
	}
	return unseen;
}
// One poll: every room the game keeps streamed has been seen, the one under
// the player stood in, and on open ground the frontier quadrants are current.
void Track(const D2RL::PluginContext* context, const Lifecycle& state, uint64_t now) {
	NativeLevelExits::Layout native;
	if (!NativeEntities::ReadLayout(state.player, false, native)) {
		ExplorationCueService::Silence();
		return;
	}
	LoadLevel(context, native);
	for (const auto& room : native.rooms)
		if (room.active) g_explored.Mark(native.level, room.rect.x, room.rect.y);
	Radar::World world;
	const bool located = NativeWorld::Read(state.player, state.session, state.level, world)
		&& static_cast<uint32_t>(world.level) == native.level;
	// Stored visits must be loaded before a room can be called unexplored.
	if (located && g_roomSounds && (g_loadedLevel == native.level || g_directory.empty()))
		ExplorationCueService::Update(context, native, world.player, g_explored, RoomService::Maze(native.level));
	else ExplorationCueService::Silence();
	if (located) {
		const int tileX = static_cast<int>(std::floor(world.player.x / LevelExitLayout::SubtilesPerTile));
		const int tileY = static_cast<int>(std::floor(world.player.y / LevelExitLayout::SubtilesPerTile));
		for (const auto& room : native.rooms) {
			const auto& r = room.rect;
			if (tileX < r.x || tileY < r.y || tileX >= r.x + r.width || tileY >= r.y + r.height) continue;
			// The radar keys rooms by their collision grid origin, the memory by the
			// logical rect: once per level, check that they are the same tile.
			const int index = world.RoomAt(static_cast<int>(world.player.x), static_cast<int>(world.player.y));
			if (index >= 0 && g_checkedLevel != native.level) {
				const auto& grid = world.rooms[static_cast<size_t>(index)];
				g_checkedLevel = native.level;
				if (grid.x != r.x * LevelExitLayout::SubtilesPerTile || grid.y != r.y * LevelExitLayout::SubtilesPerTile)
					AsyncLog::WarnF(context, "[terrain] level=%u collision grid origin %d,%d is not the room rect origin %d,%d tiles",
						native.level, grid.x, grid.y, r.x, r.y);
			}
			if (g_explored.Visit(native.level, r.x, r.y))
				AsyncLog::InfoF(context, "[terrain] visited room=%d,%d level=%u visited=%zu", r.x, r.y, native.level,
					g_explored.VisitedCount(native.level));
			break;
		}
	}
	uint64_t frontierMs = 0;
	if (located && RoomService::Outdoor(state.level)) {
		const uint64_t started = GetTickCount64();
		g_frontier = Frontier::Quadrants(world, Unseen(native));
		g_frontierLevel = state.level;
		frontierMs = GetTickCount64() - started;
	} else {
		g_frontier = {};
		g_frontierLevel = 0;
	}
	if (now >= g_nextSave) { SaveAll(context); g_nextSave = now + SaveIntervalMs; }
	const size_t explored = g_explored.Count(native.level);
	const size_t step = std::max<size_t>(1, native.rooms.size() * LogGrowthPercent / 100);
	if (native.level == g_loggedLevel && explored < g_loggedRooms + step) return;
	g_loggedLevel = native.level;
	g_loggedRooms = explored;
	AsyncLog::InfoF(context, "[terrain] explored rooms=%zu/%zu level=%u frontier=%u%u%u%u frontier_ms=%llu", explored, native.rooms.size(),
		native.level, g_frontier[0] ? 1u : 0u, g_frontier[1] ? 1u : 0u, g_frontier[2] ? 1u : 0u, g_frontier[3] ? 1u : 0u,
		static_cast<unsigned long long>(frontierMs));
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
	g_roomSounds = RadarConfig::ReadEnabled(context, "[exploration]").value_or(true);
	// Next to the plugin, like the code dump and the sounds: `d2r-access\explored`.
	if (context->pluginDirectory != nullptr) {
		g_directory = std::filesystem::path(context->pluginDirectory) / "d2r-access" / "explored";
		(void)g_store.Start(g_directory);
	}
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
	AsyncLog::InfoF(context, "[terrain] supported=%u room_sounds=%u store=%s", g_supported ? 1u : 0u,
		g_roomSounds ? 1u : 0u, g_directory.string().c_str());
	return g_supported;
}

void Tick(const D2RL::PluginContext* context, bool) {
	// Both sources are consumed every tick so a latched gesture cannot fire late.
	const bool asked = g_asked.exchange(false);
	const bool clicked = InputDirection::TakeStickChord();
	const bool requested = asked || clicked;
	if (!g_supported) {
		ExplorationCueService::Silence();
		if (requested) ScreenReader::Speak(Strings::Current().terrainUnavailable, true);
		return;
	}
	const uint64_t now = GetTickCount64();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	const bool playing = state.ready && Playing();
	// An asked-for readout answers even while the UI reader is talking.
	if (requested && playing) {
		std::lock_guard lock(g_mutex);
		Overview(context, state);
	}
	if (!playing) {
		std::lock_guard lock(g_mutex);
		ExplorationCueService::Silence();
		return;
	}
	if (now - g_nextPoll < PollMs) return;
	g_nextPoll = now;
	std::lock_guard lock(g_mutex);
	Track(context, state, now);
}

void Qualify(Radar::World& world) {
	std::lock_guard lock(g_mutex);
	const auto level = static_cast<uint32_t>(world.level);
	// The radar's silent baseline must see the level's memory, so the level is
	// loaded from here too when the terrain poll has not got to it yet.
	if (g_loadedLevel != level && g_state.ready && GetTickCount64() >= g_nextLoadTry) {
		NativeLevelExits::Layout native;
		if (NativeEntities::ReadLayout(g_state.player, false, native) && native.level == level) LoadLevel(g_context, native);
		else g_nextLoadTry = GetTickCount64() + PollMs;
	}
	if (g_loadedLevel != level) {
		// The first radar baseline must not call stored paths unexplored before
		// the worker has loaded their memory. These values are for this copy only.
		for (auto& room : world.rooms) room.visited = true;
		for (auto& exit : world.exits) exit.unexplored = false;
		world.frontier = {};
		return;
	}
	for (auto& room : world.rooms)
		room.visited = g_explored.Visited(level, room.x / LevelExitLayout::SubtilesPerTile, room.y / LevelExitLayout::SubtilesPerTile);
	for (auto& exit : world.exits) exit.unexplored = exit.level != 0 && !g_explored.Known(exit.level);
	world.frontier = g_frontierLevel == world.level ? g_frontier : Radar::Marks{};
}

bool Visited(int level, int tileX, int tileY) {
	std::lock_guard lock(g_mutex);
	return g_explored.Visited(static_cast<uint32_t>(level), tileX, tileY);
}

std::vector<NativeRooms::Room> VisitedRooms(int level, const std::vector<NativeRooms::Room>& rooms) {
	std::lock_guard lock(g_mutex);
	std::vector<NativeRooms::Room> visited;
	if (level <= 0 || g_loadedLevel != static_cast<uint32_t>(level)
		|| g_state.level != level || rooms.empty()) return visited;
	const auto key = g_keys.find(static_cast<uint32_t>(level));
	if (key == g_keys.end()) return visited;
	std::vector<NativeLevelExits::LevelRoom> layout;
	layout.reserve(rooms.size());
	for (const auto& room : rooms)
		layout.push_back({{room.tileX, room.tileY, room.tileWidth, room.tileHeight}, false});
	if (ExploredStore::LayoutDigest(layout) != key->second.digest) return visited;
	for (const auto& room : rooms)
		if (g_explored.Visited(static_cast<uint32_t>(level), room.tileX, room.tileY)) visited.push_back(room);
	return visited;
}

bool Known(int level) {
	std::lock_guard lock(g_mutex);
	return g_explored.Known(static_cast<uint32_t>(level));
}

void Unregister() {
	if (g_lifecycle) for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	g_listeners.clear();
	if (g_input) for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_actions.clear();
	{
		std::lock_guard lock(g_mutex);
		ExplorationCueService::Silence();
		SaveAll(g_context);
	}
	g_store.Stop(); // final worker write before the plugin context goes away
	if (g_store.Failed()) AsyncLog::Warn(g_context, "[terrain] exploration archive write failed; legacy and last good archive remain intact.");
	std::lock_guard lock(g_mutex);
	g_explored.Forget();
	g_keys.clear();
	g_state = {};
	g_asked = false;
	g_supported = false;
	g_roomSounds = true;
	g_nextPoll = 0;
	g_nextSave = 0;
	g_loadedLevel = 0;
	g_frontier = {};
	g_frontierLevel = 0;
	g_loggedLevel = 0;
	g_loggedRooms = 0;
	g_warnedLevel = 0;
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
}
}
