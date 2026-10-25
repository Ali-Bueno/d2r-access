#include "terrain_service.h"
#include "async_log.h"
#include "process_lifetime.h"
#include "entity_radar.h"
#include "explored_rooms.h"
#include "exploration_cue_service.h"
#include "explored_store.h"
#include "frontier.h"
#include "input_direction.h"
#include "item_audio.h"
#include "level_exit_layout.h"
#include "map_survey.h"
#include "native_automap.h"
#include "native_drlg.h"
#include "native_entities.h"
#include "tomb_symbols.h"
#include "native_level_exits.h"
#include "native_world.h"
#include "navigation_policy.h"
#include "radar.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "room_service.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "terrain_overview.h"
#include "tutorial.h"

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
bool g_supported = false, g_roomSounds = true, g_placeAlerts = true;
// The level being surveyed and the one before it: the way back is not a find.
int g_surveyedLevel = 0, g_cameFrom = 0;
// Finds made just after arriving wait behind the area's banner, which interrupts (a tester,
// 2026-10-20: the Canyon of the Magi's waypoint, beside the arrival, was never heard).
constexpr uint64_t ArrivalHoldMs = 2500;
uint64_t g_arrivedAt = 0;
std::vector<std::string> g_heldFinds;
// The store worker owns archive I/O; gameplay only submits room snapshots.
std::filesystem::path g_directory;
// Explicit Unregister stops/flushes the store; forced process exit must not
// run its joining destructor under the Windows loader lock.
ProcessLifetime<ExploredStore::Store> g_storeOwner;
ExploredStore::Store& g_store = g_storeOwner.Get();
std::map<uint32_t, ExploredStore::Key> g_keys;
// Fixed places found per map; stored ones join when the level's record loads.
std::map<ExploredStore::Key, std::vector<ExploredStore::Place>> g_places;
// Quest objects seen gone this run: the archive only adds places, so a level loaded again keeps them out.
std::map<ExploredStore::Key, std::vector<uint64_t>> g_gonePlaces;
uint32_t g_loadedLevel = 0;
uint64_t g_nextLoadTry = 0; // the radar asks every poll; the layout is read at most this often
uint32_t g_checkedLevel = 0; // grid-origin cross-check done for this level
// The frontier quadrants of the level the radar is qualified against.
Frontier::FrameMarks g_frontier{};
int g_frontierLevel = 0;

// The tiles the automap can draw in every room the game has streamed, by level then
// room identity, kept after the room unloads. Cleared with the exploration memory.
std::map<uint32_t, std::map<uint64_t, std::vector<AutomapCells::Point>>> g_drawable;

// Caller holds g_mutex. Records the drawable tiles of each streamed room not yet measured.
void RecordDrawable() {
	for (const uintptr_t room : NativeWorld::LoadedRooms()) {
		const auto unmeasured = [](const NativeAutomap::RoomTiles& found) {
			const auto level = g_drawable.find(found.level);
			return level == g_drawable.end() || !level->second.contains(NativeRooms::Identity(found.tileX, found.tileY));
		};
		if (auto found = NativeAutomap::DrawableTiles(room, unmeasured))
			g_drawable[found->level].emplace(NativeRooms::Identity(found->tileX, found->tileY), std::move(found->tiles));
	}
}
// Caller holds g_mutex.
void SaveAll(const D2RL::PluginContext* context) {
	if ((!g_explored.Dirty() && g_places.empty()) || g_directory.empty()) return;
	std::vector<ExploredStore::Update> updates;
	for (const auto& [level, key] : g_keys) {
		const auto places = g_places.find(key);
		updates.push_back({key, g_explored.Snapshot(level), places == g_places.end() ? std::vector<ExploredStore::Place>{} : places->second});
	}
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
	std::vector<ExploredStore::Place> stored;
	const bool loaded = g_store.Load(key, g_explored, &stored);
	auto& places = g_places[key];
	ExploredStore::MergePlaces(stored, places); // found this game is newer than the disk
	places = std::move(stored);
	if (const auto gone = g_gonePlaces.find(key); gone != g_gonePlaces.end())
		std::erase_if(places, [&](const auto& place) { return std::ranges::find(gone->second, place.identity) != gone->second.end(); });
	g_loadedLevel = native.level;
	AsyncLog::InfoF(context, "[terrain] level=%u seed=%08X rooms=%zu %s %s: seen=%zu visited=%zu places=%zu", native.level, seed,
		native.rooms.size(), loaded ? "loaded" : "new", ExploredStore::ArchiveFile(g_directory).filename().string().c_str(),
		g_explored.Count(native.level), g_explored.VisitedCount(native.level), places.size());
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
		g_places.clear();
		g_drawable.clear();
		g_surveyedLevel = g_cameFrom = 0;
		g_heldFinds.clear();
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
// Caller holds g_mutex. The key of the loaded record for `level`, when the
// player is there and `rooms` are that record's layout.
const ExploredStore::Key* LoadedKey(int level, const std::vector<NativeRooms::Room>& rooms) {
	if (level <= 0 || g_loadedLevel != static_cast<uint32_t>(level) || g_state.level != level || rooms.empty()) return nullptr;
	const auto key = g_keys.find(static_cast<uint32_t>(level));
	if (key == g_keys.end()) return nullptr;
	std::vector<NativeLevelExits::LevelRoom> layout;
	layout.reserve(rooms.size());
	for (const auto& room : rooms)
		layout.push_back({{room.tileX, room.tileY, room.tileWidth, room.tileHeight}, false});
	return ExploredStore::LayoutDigest(layout) == key->second.digest ? &key->second : nullptr;
}
// VisitedRooms with g_mutex already held.
std::vector<NativeRooms::Room> VisitedRoomsLocked(int level, const std::vector<NativeRooms::Room>& rooms) {
	std::vector<NativeRooms::Room> visited;
	if (!LoadedKey(level, rooms)) return visited;
	for (const auto& room : rooms)
		if (g_explored.Visited(static_cast<uint32_t>(level), room.tileX, room.tileY)) visited.push_back(room);
	return visited;
}
std::vector<EntityRadar::Entity> KnownPlacesLocked(int level, const std::vector<NativeRooms::Room>& rooms);
// One spoken summary of the level around the player.
void Overview(const D2RL::PluginContext* context, const Lifecycle& state) {
	Tutorial::Post(Tutorial::Signal::Terrain);
	const auto& strings = Strings::Current();
	Radar::World world;
	NativeLevelExits::Layout native;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)
		|| !NativeEntities::ReadLayout(state.player, true, native)) {
		ScreenReader::Speak(strings.terrainUnavailable, true);
		AsyncLog::InfoF(context, "[terrain] layout unavailable level=%d", state.level);
		return;
	}
	std::vector<NativeRooms::Room> rooms;
	rooms.reserve(native.rooms.size());
	for (const auto& room : native.rooms)
		rooms.push_back({room.rect.x, room.rect.y, room.rect.width, room.rect.height, NativeRooms::Identity(room.rect.x, room.rect.y)});
	// Explored as the tracker and the beacon count it: stood in or drawn on the automap.
	// Not every room the game streamed: outdoors that is most of the level, and the
	// overview said 100 percent with a dozen rooms still blank (a tester, 2026-10-17).
	const auto charted = NativeAutomap::RevealedRooms(rooms);
	const auto explored = NavigationPolicy::Explored(VisitedRoomsLocked(state.level, rooms), charted ? &*charted : nullptr, rooms, world.player);
	RecordDrawable();
	// The spoken share counts the cells the automap has drawn over those it can draw (a tester,
	// 2026-10-24: walkable tiles never filled up); without them it falls back to rooms.
	const auto& measured = g_drawable[native.level];
	std::vector<std::vector<AutomapCells::Point>> listed;
	listed.reserve(rooms.size());
	for (const auto& room : rooms) {
		if (const auto found = measured.find(room.id); found != measured.end()) {
			listed.push_back(found->second);
			continue;
		}
		// Not streamed this game: whatever the automap kept of it counts as drawn.
		std::vector<AutomapCells::Point> area;
		for (int y = room.tileY; y < room.tileY + room.tileHeight; ++y)
			for (int x = room.tileX; x < room.tileX + room.tileWidth; ++x) area.push_back({x, y});
		listed.push_back(std::move(area));
	}
	const auto drawn = NativeAutomap::DrawnTiles(listed);
	TerrainOverview::Layout layout;
	layout.rooms.reserve(native.rooms.size());
	for (size_t i = 0; i < rooms.size(); ++i) {
		const auto& room = rooms[i];
		TerrainOverview::Room entry{{room.tileX, room.tileY, room.tileWidth, room.tileHeight},
			std::any_of(explored.begin(), explored.end(), [&](const NativeRooms::Room& known) { return known.id == room.id; })};
		if (drawn) {
			entry.measured = measured.contains(room.id);
			entry.drawable = entry.measured ? listed[i].size() : 0;
			entry.drawn = (*drawn)[i];
		}
		layout.rooms.push_back(entry);
	}
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
	// The places the scanner has logged here, as the automap shows them (a tester, 2026-10-14).
	std::vector<TerrainOverview::Landmark> landmarks;
	for (const auto& place : KnownPlacesLocked(state.level, rooms))
		landmarks.push_back({place.name, {place.position.x - world.player.x, place.position.y - world.player.y}});
	const auto text = TerrainOverview::Speak(summary, LevelName(native.level, strings), g_scan, strings, landmarks);
	ScreenReader::Speak(text, true);
	std::string legs;
	for (const auto& leg : summary.legs) {
		char entry[96]{};
		std::snprintf(entry, sizeof entry, " edge=%.0f unexplored=%.0f border=%s", leg.edge,
			leg.hasUnexplored ? leg.unexplored : 0.0, leg.border.empty() ? "-" : leg.border.c_str());
		legs += entry;
	}
	AsyncLog::InfoF(context, "[terrain] at=%.0f,%.0f level=%u box=%d,%d %dx%d place=%zu rooms=%zu/%zu (charted %zu, streamed %zu) percent=%d (drawn %zu of %zu drawable tiles, estimated %zu%s) landmarks=%zu%s -> %s",
		world.player.x, world.player.y, native.level, summary.box.x, summary.box.y,
		summary.box.width, summary.box.height, summary.place, explored.size(), native.rooms.size(),
		charted ? charted->size() : 0, g_explored.Count(native.level), summary.percent, summary.drawnTiles, summary.drawableTiles, summary.estimatedTiles,
		summary.byTiles ? "" : ", by rooms", landmarks.size(), legs.c_str(), text.c_str());
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
		RecordDrawable();
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
	const auto& cardinal = g_frontier[static_cast<size_t>(Radar::Frame::Cardinal)];
	const auto& diagonal = g_frontier[static_cast<size_t>(Radar::Frame::Diagonal)];
	AsyncLog::InfoF(context, "[terrain] explored rooms=%zu/%zu level=%u frontier=%u%u%u%u diagonal=%u%u%u%u frontier_ms=%llu", explored,
		native.rooms.size(), native.level, cardinal[0] ? 1u : 0u, cardinal[1] ? 1u : 0u, cardinal[2] ? 1u : 0u, cardinal[3] ? 1u : 0u,
		diagonal[0] ? 1u : 0u, diagonal[1] ? 1u : 0u, diagonal[2] ? 1u : 0u, diagonal[3] ? 1u : 0u, static_cast<unsigned long long>(frontierMs));
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
	g_placeAlerts = RadarConfig::ReadEnabled(context, "[discoveries]").value_or(true);
	// Next to the plugin, like the code dump and the sounds: `d2r-access\explored`.
	if (context->pluginDirectory != nullptr) {
		g_directory = std::filesystem::path(context->pluginDirectory) / "d2r-access" / "explored";
		(void)g_store.Start(g_directory);
	}
	(void)context->RegisterConsoleCommand("access-terrain", Console,
		"Terrain overview: how far the level reaches and what is unexplored. Ctrl+O, or BACK+B on the pad.");
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		// Keyboard equivalent of BACK+B.
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
	const bool clicked = InputDirection::TakeBackWith(InputDirection::FaceButton::B);
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

void Qualify(Radar::World& world, Radar::Frame frame) {
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
	world.frontier = g_frontierLevel == world.level ? g_frontier[static_cast<size_t>(frame)] : Radar::Marks{};
}

bool Visited(int level, int tileX, int tileY) {
	std::lock_guard lock(g_mutex);
	return g_explored.Visited(static_cast<uint32_t>(level), tileX, tileY);
}

std::vector<NativeRooms::Room> VisitedRooms(int level, const std::vector<NativeRooms::Room>& rooms) {
	std::lock_guard lock(g_mutex);
	return VisitedRoomsLocked(level, rooms);
}

std::vector<EntityRadar::Entity> KnownPlaces(int level, const std::vector<NativeRooms::Room>& rooms) {
	std::lock_guard lock(g_mutex);
	return KnownPlacesLocked(level, rooms);
}
namespace {
// KnownPlaces with g_mutex already held (the overview runs under it).
std::vector<EntityRadar::Entity> KnownPlacesLocked(int level, const std::vector<NativeRooms::Room>& rooms) {
	std::vector<EntityRadar::Entity> result;
	const auto* key = LoadedKey(level, rooms);
	const auto places = key ? g_places.find(*key) : g_places.end();
	if (places == g_places.end()) return result;
	for (const auto& place : places->second) {
		EntityRadar::Entity entity{place.id, static_cast<EntityRadar::Kind>(place.kind), {place.x, place.y}, place.name,
			place.identity, false, static_cast<EntityRadar::Category>(place.category), place.destination};
		// A quest object found in an earlier game is listed again, as the automap still shows
		// it (a tester, 2026-10-14); it was kept only to be announced once before.
		entity.questObject = entity.kind == EntityRadar::Kind::Object && entity.category == EntityRadar::Category::Quest;
		if (!entity.questObject && !MapSurvey::Fixed(entity)) continue; // written by a build that kept something else
		// The name in the language being played, whichever one it was found in.
		entity.nameKey = place.nameKey;
		if (auto text = NativeEntities::KeyText(place.nameKey); !text.empty()) entity.name = std::move(text);
		if (entity.name.empty()) entity.name = Strings::Current().exitUnnamed;
		TombSymbols::Mark(entity, Strings::Current());
		result.push_back(std::move(entity));
	}
	return result;
}
}

std::map<int, std::vector<EntityRadar::Entity>> MapPlaces(uint32_t seed) {
	std::lock_guard lock(g_mutex);
	auto stored = g_store.AllKnownPlaces(seed);
	for (const auto& [key, places] : g_places) if (key.seed == seed) ExploredStore::MergePlaces(stored[key.level], places);
	std::map<int, std::vector<EntityRadar::Entity>> result;
	for (const auto& [level, places] : stored) for (const auto& place : places) {
		EntityRadar::Entity entity{place.id, static_cast<EntityRadar::Kind>(place.kind), {place.x, place.y}, place.name,
			place.identity, false, static_cast<EntityRadar::Category>(place.category), place.destination};
		if (!MapSurvey::Fixed(entity)) continue;
		entity.nameKey = place.nameKey;
		if (auto name = NativeEntities::KeyText(place.nameKey); !name.empty()) entity.name = std::move(name);
		TombSymbols::Mark(entity, Strings::Current());
		result[static_cast<int>(level)].push_back(std::move(entity));
	}
	return result;
}

void RememberPlaces(int level, const std::vector<NativeRooms::Room>& rooms, const std::vector<EntityRadar::Entity>& found) {
	// Stored by its plain name, said by the scanner's (a tomb with its symbol: a tester, 2026-10-20).
	struct Found { ExploredStore::Place place; bool quest; std::string label; };
	std::vector<Found> fixed;
	for (const auto& entity : found) {
		const bool quest = EntityRadar::QuestSource(entity);
		if (!quest && !MapSurvey::Fixed(entity)) continue;
		// A quest object is known by where it stands; a Cairn Stone by its name without the symbol.
		fixed.push_back({{entity.id, entity.placeId ? entity.placeId : NativeLevelExits::Identity(entity.position),
			entity.position.x, entity.position.y, entity.destinationLevel, entity.worldName.empty() ? entity.name : entity.worldName,
			static_cast<uint8_t>(entity.kind), static_cast<uint8_t>(entity.category), entity.nameKey}, quest, entity.name});
	}
	std::vector<std::string> finds;
	{
		std::lock_guard lock(g_mutex);
		const auto* key = LoadedKey(level, rooms);
		if (!key) return;
		const uint64_t now = GetTickCount64();
		if (g_surveyedLevel != level) {
			g_cameFrom = g_surveyedLevel; g_surveyedLevel = level;
			g_arrivedAt = now;
			g_heldFinds.clear();
		}
		for (const auto& [place, quest, label] : fixed) {
			auto& known = g_places[*key];
			const auto novelty = ExploredStore::Classify(known, place, static_cast<uint32_t>(g_cameFrom));
			const bool namesake = quest && ExploredStore::Namesake(known, place);
			if (!ExploredStore::MergePlaces(known, {place}) || novelty == ExploredStore::Novelty::Known) continue;
			const bool wayBack = novelty == ExploredStore::Novelty::WayBack;
			AsyncLog::InfoF(g_context, "[terrain] place remembered level=%d at=%.0f,%.0f category=%u to=%u places=%zu name=%s%s", level,
				place.x, place.y, place.category, place.destination, known.size(), place.name.c_str(),
				wayBack ? " (way back)" : namesake ? " (quest, namesake)" : quest ? " (quest)" : "");
			if (wayBack || namesake || !g_placeAlerts) continue;
			const auto& strings = Strings::Current();
			const bool exit = static_cast<EntityRadar::Category>(place.category) == EntityRadar::Category::Exit;
			char phrase[200]{};
			std::snprintf(phrase, sizeof(phrase), quest ? strings.placeFoundQuest : exit ? strings.placeFoundExit : strings.placeFound,
				label.c_str());
			finds.emplace_back(phrase);
		}
		// They wait for the tutorial's words too, which they had broken into (the user, 2026-10-21).
		if (now - g_arrivedAt < ArrivalHoldMs || Tutorial::Busy()) {
			g_heldFinds.insert(g_heldFinds.end(), finds.begin(), finds.end());
			finds.clear();
		} else if (!g_heldFinds.empty()) {
			finds.insert(finds.begin(), g_heldFinds.begin(), g_heldFinds.end());
			g_heldFinds.clear();
		}
	}
	for (const auto& phrase : finds) ScreenReader::Speak(phrase, false);
	if (!finds.empty()) ItemAudio::PlayDiscovery();
}

void ForgetGoneQuestPlaces(int level, const std::vector<NativeRooms::Room>& rooms, const std::vector<EntityRadar::Entity>& present,
	const std::function<bool(Radar::Position)>& covered) {
	std::lock_guard lock(g_mutex);
	const auto* key = LoadedKey(level, rooms);
	const auto places = key ? g_places.find(*key) : g_places.end();
	if (places == g_places.end()) return;
	std::erase_if(places->second, [&](const ExploredStore::Place& place) {
		if (static_cast<EntityRadar::Kind>(place.kind) != EntityRadar::Kind::Object
			|| static_cast<EntityRadar::Category>(place.category) != EntityRadar::Category::Quest || !covered({place.x, place.y})) return false;
		if (std::any_of(present.begin(), present.end(), [&](const EntityRadar::Entity& entity) {
				return entity.kind == EntityRadar::Kind::Object && EntityRadar::QuestSource(entity)
					&& NativeLevelExits::Identity(entity.position) == place.identity; })) return false;
		g_gonePlaces[*key].push_back(place.identity);
		AsyncLog::InfoF(g_context, "[terrain] quest place gone level=%d at=%.0f,%.0f name=%s", level, place.x, place.y, place.name.c_str());
		return true;
	});
}

std::vector<NativeRooms::Room> ExploredRooms(int level, const std::vector<NativeRooms::Room>& rooms, Radar::Position player) {
	const auto charted = NativeAutomap::RevealedRooms(rooms);
	return NavigationPolicy::Explored(VisitedRooms(level, rooms), charted ? &*charted : nullptr, rooms, player);
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
	g_storeOwner.Reset(); // archive cache is released during normal unload, not DLL detach
	std::lock_guard lock(g_mutex);
	g_explored.Forget();
	g_keys.clear();
	g_places.clear();
	g_drawable.clear();
	g_state = {};
	g_asked = false;
	g_supported = false;
	g_roomSounds = true;
	g_placeAlerts = true;
	g_surveyedLevel = g_cameFrom = 0;
	g_heldFinds.clear();
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

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool RoomSounds() { return g_roomSounds; }
void SetRoomSounds(bool enabled) { g_roomSounds = enabled; }
bool PlaceAlerts() { return g_placeAlerts; }
void SetPlaceAlerts(bool enabled) { g_placeAlerts = enabled; }
}
