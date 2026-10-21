#include "map_survey_service.h"
#include "generated/level_flags.h"
#include "input_direction.h"

#include "async_log.h"
#include "beacon_service.h"
#include "corpse_memory.h"
#include "crossing_memory.h"
#include "death_place_service.h"
#include "map_survey.h"
#include "layout_check.h"
#include "native_drlg.h"
#include "native_actions.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "level_areas.h"
#include "native_level_areas.h"
#include "native_level_exits.h"
#include "native_rooms.h"
#include "drlg_layout.h"
#include "keyboard_menu.h"
#include "keyboard_service.h"
#include "native_world.h"
#include "place_service.h"
#include "navigation_mode.h"
#include "quest_seals.h"
#include "radar_ui.h"
#include "room_catalogue.h"
#include "screen_reader.h"
#include "entity_cue_service.h"
#include "speech_strings.h"
#include "spoken_direction.h"
#include "terrain_service.h"
#include "tutorial_service.h"
#include "zone_transitions.h"

#include <D2RLPlugin/input.h>
#include <D2RLPlugin/lifecycle_events.h>

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace D2RAccess::MapSurveyService {

namespace {

using Kind = D2RL::Lifecycle::GameplayEventKind;

// As wide as the graph can be built, which is as wide as the game keeps
// collision around the player. Taken from the cap rather than written out: a
// larger number does not widen the catalogue, it empties it -- Build returns
// an empty grid above the cap, and every object then reads as "no route".
constexpr int SurveyRadius = Navigation::MaxRadius;
// The registry is fed from the ordinary update, which must stay cheap.
constexpr uint64_t ObserveIntervalMs = 500;
// Standing within StillDistance subtiles of the last pass, the next waits StillObserveMs:
// what is in sight changes little, and enemies and items are re-read then.
constexpr double StillDistance = 1;
constexpr uint64_t StillObserveMs = 2000;
// A pass this slow names its parts in the log, at most every SlowLogMs (the pump's own threshold and pace).
constexpr double SlowPassMs = 30;
constexpr uint64_t SlowLogMs = 5000;
int g_observedLevel = 0;
uint64_t g_observedSession = 0;
Radar::Position g_observedAt{};
uint64_t g_observedTime = 0, g_nextSlowLog = 0;

struct Lifecycle {
	uint32_t player = 0;
	uint64_t session = 0;
	int      level = 0;
	bool     ready = false;
};

std::mutex                 g_mutex;
Lifecycle                  g_state;
MapSurvey::Memory          g_registry;
CorpseMemory::Memory       g_corpses;
RoomCatalogue::Registry    g_rooms;
// The level's logical rooms do not change while the level instance lives, so
// the native walk happens once per instance rather than on every observation.
std::vector<NativeRooms::Room> g_roomList;
int                        g_roomLevel = 0;
uint64_t                   g_roomSession = 0;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService*  g_input = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
bool                       g_supported = false;
uint64_t                   g_nextObserve = 0;
// The level the area list was last logged for; diagnostics only.
int                        g_areaLevel = 0;
uint64_t                   g_areaSession = 0;
// How many town NPCs were last logged, and where; diagnostics only.
int                        g_townNpcLevel = 0;
uint64_t                   g_townNpcSession = 0;
size_t                     g_townNpcCount = 0, g_townNpcSeen = 0;
MapSurvey::TownNpcSightings g_townNpcs;

// What the player is currently pointed at. Kept by identity, not index, so a
// rescan that adds or removes objects does not silently move the selection.
EntityRadar::Category g_category = EntityRadar::Category::Unknown;
uint32_t              g_selected = 0;
// A fixed place is known by where it is: an exit's id is not its own (a warp's
// row, a level's number, or a unit's id while its room is loaded). MEASURED
// 2026-09-28, Cave Level 1: every step said Cold Plains, never Cave Level 2;
// INFERRED cause, a shared or changing id, which the place does not have.
uint64_t              g_selectedPlace = 0;
// Design value: after this long without a scanner answer the list starts again at the nearest.
constexpr uint64_t    StaleSelectionMs = 10000;
uint64_t              g_answeredAt = 0;
bool Selected(const EntityRadar::Entity& entity) {
	return g_selectedPlace ? entity.placeId == g_selectedPlace : entity.id == g_selected;
}

enum class Request : int { None, Repeat, PreviousObject, NextObject, PreviousCategory, NextCategory, ReachFilter };
std::atomic<Request> g_request{ Request::None };
bool                 g_fullAssistance = true;
// The filter for what cannot be reached, and its key, which is a switch and
// works out of play too.
std::atomic<bool>    g_reachFilter{ false };
std::atomic<bool>    g_townNpcsEverywhere{ true };
std::atomic<bool>    g_reachToggle{ false };
// What the filter last left out, so the log says it once and not at every key.
size_t               g_cutLogged = 0;
int                  g_cutLevel = 0;

bool Playing(const Lifecycle& state) { return state.ready && state.level != 0; }

// The game the registry's entries were seen in. The registry clears itself
// when the level changes, but a town is the same level in every game, and a
// new game hands every NPC a new unit id -- so the last game's Akara was never
// matched again, never forgotten either (she stood some 1800 subtiles outside
// the new map), and was listed beside the real one. MEASURED 2026-09-21:
// "Akara, 6 steps, to the right" and "Akara, 1835 steps, to the left".
uint64_t g_registrySession = 0;

void ForgetOtherGames(const Lifecycle& state) {
	if (state.session == g_registrySession) return;
	AsyncLog::InfoF(g_context, "[survey] game session %llu: catalogue cleared, %zu entries from the last game dropped",
		static_cast<unsigned long long>(state.session), g_registry.Entities().size());
	g_registry.Clear();
	g_corpses.Clear();
	g_townNpcs.Clear();
	g_registrySession = state.session;
	g_selected = 0;
	g_selectedPlace = 0;
}

void ApplyMode() {
	const bool fullAssistance = NavigationMode::FullAssistance();
	if (fullAssistance == g_fullAssistance) return;
	g_fullAssistance = fullAssistance;
	g_registry.Clear();
	g_observedTime = 0; // refill the cleared catalogue at once, standing still or not
	g_selected = 0;
	g_selectedPlace = 0;
	g_category = EntityRadar::Category::Unknown;
}

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) g_state.level = event->currentValue;
}

const char* CategoryName(EntityRadar::Category category) {
	const auto& names = Strings::Current().surveyCategories;
	// MapSurvey::Categories only ever yields real categories; the table is
	// indexed by this switch, so a new category appends to the table.
	switch (category) {
	case EntityRadar::Category::Exit:         return names[0];
	case EntityRadar::Category::Portal:       return names[1];
	case EntityRadar::Category::Waypoint:     return names[2];
	case EntityRadar::Category::Npc:          return names[3];
	case EntityRadar::Category::Stash:        return names[4];
	case EntityRadar::Category::Enemy:        return names[5];
	case EntityRadar::Category::Treasure:     return names[6];
	case EntityRadar::Category::Shrine:       return names[7];
	case EntityRadar::Category::Interactable: return names[8];
	case EntityRadar::Category::Item:         return names[9];
	case EntityRadar::Category::Room:         return names[10];
	case EntityRadar::Category::Player:       return names[11];
	case EntityRadar::Category::UnexploredRoom: return names[12];
	case EntityRadar::Category::Corpse:       return names[13];
	case EntityRadar::Category::Quest:        return names[14];
	default:                                  return names[8];
	}
}

std::string Format(const char* format, auto... args) {
	char buffer[512]{};
	std::snprintf(buffer, sizeof(buffer), format, args...);
	return buffer;
}

std::string Describe(const MapSurvey::Landmark& landmark) {
	const Strings::Table& strings = Strings::Current();
	// Eight compass points or twelve hours, diagonals included (the user, 2026-09-25).
	const char* direction = SpokenDirection::Of(landmark.offset, strings);
	// Nothing here is out of reach. The grid only covers the collision window
	// the game keeps around the player, so a landmark it cannot route to is
	// almost always one the catalogue can see and the grid cannot -- the
	// catalogue is the level-wide reader precisely for that. Quote the
	// straight line, which MapSurvey::Survey fills in from world positions for
	// every landmark whether the grid saw it or not. It understates a target
	// behind a wall, but a direction and a rough distance can be acted on. Not
	// called "straight line" aloud: next to a diagonal it made no sense (the
	// user, 2026-09-25). Only a landmark on the player has no distance at all.
	if (!g_fullAssistance) {
		const std::string distance = EntityRadar::DistancePhrase(landmark.straightLine, EntityRadar::Settings{}, strings);
		return Format(strings.surveySteps, EntityRadar::SpokenName(landmark.entity, strings.listSeparator).c_str(), distance.c_str(), direction);
	}
	const double reach = landmark.reachable ? static_cast<double>(landmark.steps) : landmark.straightLine;
	if (!landmark.reachable && landmark.straightLine <= 0) {
		return Format(strings.surveyBearing, EntityRadar::SpokenName(landmark.entity, strings.listSeparator).c_str(), direction);
	}
	// Spoken like the scan's distance. For a reachable landmark it is the
	// WALKING distance -- the steps the game's own graph needs to get there --
	// and a wall between the two makes the numbers differ.
	const std::string distance = EntityRadar::DistancePhrase(reach, EntityRadar::Settings{}, strings);
	return Format(strings.surveySteps, EntityRadar::SpokenName(landmark.entity, strings.listSeparator).c_str(), distance.c_str(), direction);
}

// What the act has built around this level, once per level. The exits
// category is fed from this: a town has no warp of its own, so the only way
// out it can offer is the area next door. If a neighbour is missing from the
// list there is nothing to point at, and this line says which of the two
// happened rather than leaving an empty category to be guessed about.
void LogAreas(const D2RL::PluginContext* context, const Lifecycle& state) {
	if (state.level == g_areaLevel && state.session == g_areaSession) return;
	g_areaLevel = state.level; g_areaSession = state.session;
	std::vector<NativeLevelAreas::Area> areas;
	if (!NativeDrlg::Available() || !NativeLevelAreas::Read(NativeWorld::PlayerRoom(), areas)) {
		AsyncLog::Info(context, "[areas] unavailable");
		return;
	}
	std::string built;
	for (const auto& area : areas) {
		char entry[80]{};
		std::snprintf(entry, sizeof(entry), " %u@%d,%d %dx%d/%zur", area.level,
			area.tileX, area.tileY, area.tileWidth, area.tileHeight, area.rooms);
		built += entry;
	}
	AsyncLog::InfoF(context, "[areas] level %d, %zu built:%s", state.level, areas.size(), built.c_str());
	for (const auto& crossing : LevelAreas::Neighbours(areas, static_cast<uint32_t>(state.level)))
		AsyncLog::InfoF(context, "[areas] crossing to level %u at %.0f,%.0f gap=%d tiles",
			crossing.level, crossing.position.x, crossing.position.y, crossing.gapTiles);
}

// Rooms of the level being played, for what the automap has charted. Empty
// when they cannot be read, which leaves exploration to line of sight alone.
const std::vector<NativeRooms::Room>& CurrentRooms(int level, uint64_t session) {
	if (g_roomLevel != level || g_roomSession != session) {
		// A failed read is tried again: the player may be between levels.
		if (NativeRooms::Read(NativeWorld::PlayerRoom(), g_roomList, level)) {
			g_roomLevel = level;
			g_roomSession = session;
		}
	}
	return g_roomList;
}

using MapSurvey::Scene;

// The town NPCs listed from afar, whenever their number changes: whether every
// NPC of the town has a spawn point the reader finds (docs/entity-scan.md).
void LogTownNpcs(const D2RL::PluginContext* context, const Lifecycle& state, const Scene& scene) {
	const size_t seen = g_townNpcs.Seen(state.level, scene.townNpcs).size();
	if (state.level == g_townNpcLevel && state.session == g_townNpcSession && scene.townNpcs.size() == g_townNpcCount
		&& seen == g_townNpcSeen) return;
	g_townNpcLevel = state.level; g_townNpcSession = state.session; g_townNpcCount = scene.townNpcs.size(); g_townNpcSeen = seen;
	if (scene.townNpcs.empty()) return;
	std::string listed;
	for (const auto& npc : scene.townNpcs) {
		char entry[96]{};
		std::snprintf(entry, sizeof(entry), " %s@%.0f,%.0f/%s", npc.name.c_str(), npc.position.x, npc.position.y,
			npc.id == npc.placeId ? "spawn" : "unit");
		listed += entry;
	}
	AsyncLog::InfoF(context, "[survey] town NPCs on level %d: %zu, %zu seen:%s; presets row x count:%s", state.level,
		scene.townNpcs.size(), seen, listed.c_str(), NativeEntities::TownPresets().c_str());
}

bool Collect(const Lifecycle& state, Scene& scene) {
	std::vector<EntityRadar::Entity> entities;
	if (!NativeEntities::Read(state.player, state.session, entities, true, true, true, true)) return false;
	// The map's own landmarks with no unit: the desert's cliff stairs (a tester, 2026-10-17).
	const auto mapLandmarks = PlaceService::MapLandmarks();
	entities.insert(entities.end(), mapLandmarks.begin(), mapLandmarks.end());
	scene = MapSurvey::Split(std::move(entities));
	return true;
}

// The rooms whose landmarks exploration has found: those stood in, those the
// automap has drawn, and the one the player stands in. A sighted player sees a
// room whole on stepping into it, not only from its middle (the user, 2026-10-14).
std::vector<NativeRooms::Room> Explored(const Lifecycle& state, const Radar::World& world) {
	return TerrainService::ExploredRooms(state.level, CurrentRooms(state.level, state.session), world.player);
}
// The room the player stands in, whose live contents are all in sight.
std::vector<NativeRooms::Room> Here(const Lifecycle& state, const Radar::World& world) {
	const auto& rooms = CurrentRooms(state.level, state.session);
	const size_t room = NativeRooms::Containing(rooms, world.player);
	return room == NativeRooms::None ? std::vector<NativeRooms::Room>{} : std::vector<NativeRooms::Room>{rooms[room]};
}

// What exploration counts as found: what came into view and anything in an
// explored room. An area border also counts where it is walked across, not
// only at the middle of the edge.
std::vector<EntityRadar::Entity> Discovered(const Lifecycle& state, const Radar::World& world, const Scene& scene) {
	const auto explored = Explored(state, world);
	// A town is small and safe: its ways out are listed from anywhere in it, as its NPCs
	// are (a tester, 2026-10-14: the camp's exit was missing in a fresh multiplayer game).
	const bool town = g_townNpcsEverywhere.load() && Generated::FindLevelFlags(state.level).town;
	const ZoneTransitions::Filter found = [&](const std::vector<EntityRadar::Entity>& entities) {
		auto result = MapSurvey::Charted(world, entities, explored);
		if (town) for (const auto& entity : entities)
			if (entity.kind == EntityRadar::Kind::Exit && std::none_of(result.begin(), result.end(),
					[&](const auto& known) { return EntityRadar::SameTarget(known, entity); })) result.push_back(entity);
		return result;
	};
	NativeLevelExits::Layout layout;
	if (std::none_of(scene.landmarks.begin(), scene.landmarks.end(), ZoneTransitions::Crosses)
		|| !NativeEntities::ReadLayout(state.player, true, layout)) return found(scene.landmarks);
	const auto crossings = ZoneTransitions::Find(world, layout, Navigation::MaxRadius);
	CrossingMemory::Note(state.session, state.level, crossings);
	return ZoneTransitions::FoundAtCrossings(scene.landmarks, crossings, found);
}

// Enemies are never charted: in range and in sight right now, or in the room the
// player stands in, or all loaded with full assistance.
std::vector<EntityRadar::Entity> Enemies(const Radar::World& world, const Scene& scene, const std::vector<NativeRooms::Room>& here) {
	return g_fullAssistance ? scene.enemies : MapSurvey::Charted(world, scene.enemies, here);
}

// Items on the ground in every loaded room of the level, however far.
std::vector<EntityRadar::Entity> LoadedItems(const Lifecycle& state) {
	std::vector<EntityRadar::Entity> items;
	GroundItems::Snapshot snapshot;
	if (!NativeGroundItems::ReadLevel(state.player, state.session, snapshot)) return items;
	for (const auto& item : snapshot.items) {
		if (item.source != GroundItems::SourceKind::Item || item.name.empty()) continue;
		EntityRadar::Entity entity;
		entity.id = item.id;
		entity.kind = EntityRadar::Kind::Item;
		entity.category = EntityRadar::Category::Item;
		entity.position = {item.position.x, item.position.y};
		entity.name = item.name;
		entity.unit = static_cast<uint8_t>(NativeActions::Unit::Item);
		items.push_back(std::move(entity));
	}
	return items;
}

// Full assistance lists everything loaded. Only what was discovered is kept
// for this map's later games, in either mode.
// Enemies and items are remembered as every other thing is (the user,
// 2026-09-28: everything discovered stays), and forgotten the same way: once
// the map can see where one was and it is not there -- killed, picked up.
// An enemy is also forgotten the moment the game holds it dead, wherever it lay.
void Observe(const Lifecycle& state, const Radar::World& world, const Navigation::Grid& grid, const Scene& scene) {
	const auto& rooms = CurrentRooms(state.level, state.session);
	DeathPlaceService::Track(NativeDrlg::MapSeed(NativeWorld::PlayerRoom(), state.level), state.level, world.player);
	const auto discovered = Discovered(state, world, scene);
	g_townNpcs.Found(state.level, MapSurvey::Charted(world, scene.townNpcs, Explored(state, world)));
	const auto items = LoadedItems(state);
	std::vector<EntityRadar::Entity> seen = g_fullAssistance ? scene.landmarks : discovered;
	const auto here = Here(state, world);
	const auto enemies = Enemies(world, scene, here);
	seen.insert(seen.end(), enemies.begin(), enemies.end());
	const auto found = g_fullAssistance ? items : MapSurvey::Charted(world, items, here);
	seen.insert(seen.end(), found.begin(), found.end());
	std::vector<EntityRadar::Entity> present = scene.landmarks;
	present.insert(present.end(), scene.enemies.begin(), scene.enemies.end());
	present.insert(present.end(), items.begin(), items.end());
	g_registry.Observe(state.level, seen, grid, !g_fullAssistance, {}, &present);
	g_registry.ForgetEnemies(NativeEntities::TakeDeadMonsters());
	KeyboardService::NotePads(world, scene.landmarks);
	// A border is kept where it is walked across (the corridor mouth), not at the point that names it.
	auto remembered = discovered;
	for (auto& entity : remembered)
		if (ZoneTransitions::Crosses(entity))
			if (const auto known = CrossingMemory::Find(state.session, state.level, entity.destinationLevel, world.player)) entity.position = *known;
	TerrainService::RememberPlaces(state.level, rooms, remembered);
	LogTownNpcs(g_context, state, scene);
	const auto covered = [&](Radar::Position position) { return CorpseMemory::Covered(rooms, grid, world.player, position); };
	TerrainService::ForgetGoneQuestPlaces(state.level, rooms, scene.landmarks, covered);
	g_corpses.Observe(state.level, scene.corpses, covered);
	std::vector<CorpseMemory::Owner> owners;
	if (NativeEntities::ReadRosterCorpses(state.player, owners)) g_corpses.Track(owners, Strings::Current().corpseOf);
}

// The filter for what cannot be reached. 2026-10-02, the Arcane Sanctuary: its
// walkways are joined by teleport pads alone, and the scanner listed all twenty
// pads loaded, 34 walks to one ending with no way. Left out: what the loaded floor
// has no steps to and the map of the level shows cut off from where the player
// stands. What cannot be told stays listed -- a place past ground the game has
// not shown yet, or past a door, which can be opened.
void LeaveOutCutOff(const Radar::World& world, std::vector<MapSurvey::Landmark>& landmarks) {
	std::vector<const EntityRadar::Entity*> things;
	things.reserve(landmarks.size());
	for (const auto& landmark : landmarks) things.push_back(&landmark.entity);
	const auto cut = KeyboardService::CutOff(world, things);
	std::vector<MapSurvey::Landmark> listed;
	listed.reserve(landmarks.size());
	std::string names;
	size_t out = 0;
	constexpr size_t Named = 12; // enough to find a thing left out wrongly; the rest are counted
	for (size_t i = 0; i < landmarks.size(); ++i) {
		if (landmarks[i].reachable || !cut[i]) { listed.push_back(std::move(landmarks[i])); continue; }
		const auto& entity = landmarks[i].entity;
		if (out++ < Named) names += Format(" %s@%.0f,%.0f", entity.name.c_str(), entity.position.x, entity.position.y);
	}
	if (out != g_cutLogged || world.level != g_cutLevel) {
		g_cutLogged = out;
		g_cutLevel = world.level;
		AsyncLog::InfoF(g_context, "[survey] reach filter: %zu of %zu cut off from %.0f,%.0f and left out:%s%s",
			out, landmarks.size(), world.player.x, world.player.y, names.c_str(), out > Named ? " ..." : "");
	}
	landmarks = std::move(listed);
}

// Reads the world fresh: the answer must reflect where the player is standing
// now, not where they were when the registry was last fed.
bool CurrentSurvey(std::vector<MapSurvey::Landmark>& landmarks, int& level) {
	ApplyMode();
	Lifecycle state;
	{
		std::lock_guard lock(g_mutex);
		state = g_state;
	}
	level = state.level;
	if (!g_supported || !Playing(state)) return false;
	ForgetOtherGames(state);
	g_registry.Select(state.level, !g_fullAssistance);
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return false;
	const Navigation::Grid grid = Navigation::Build(world, SurveyRadius, Radar::Layer::Movement, false);
	Scene scene;
	if (Collect(state, scene)) Observe(state, world, grid, scene);
	g_rooms.Observe(state.level, NativeDrlg::MapSeed(NativeWorld::PlayerRoom(), state.level),
		CurrentRooms(state.level, state.session), world.player);
	std::vector<EntityRadar::Entity> catalogued = g_registry.Entities();
	// Fixed places and quest objects found on this same map in an earlier game, whose
	// rooms the game has not streamed again; the live read wins where both have one.
	// A live object has no place id: a kept quest object is it when it stands on its spot.
	for (auto& place : TerrainService::KnownPlaces(state.level, CurrentRooms(state.level, state.session)))
		if (std::none_of(catalogued.begin(), catalogued.end(), [&](const auto& known) {
				return EntityRadar::SameTarget(known, place) || (place.questObject && known.kind == place.kind
					&& NativeLevelExits::Identity(known.position) == place.placeId); }))
			catalogued.push_back(std::move(place));
	// Other players are listed whenever loaded, in sight or not: finding a teammate is the point.
	catalogued.insert(catalogued.end(), scene.players.begin(), scene.players.end());
	const auto townNpcs = g_townNpcs.Listed(state.level, scene.townNpcs, g_fullAssistance || g_townNpcsEverywhere);
	catalogued.insert(catalogued.end(), townNpcs.begin(), townNpcs.end());
	// Corpses from anywhere on the level: once seen, one stays until its place is seen without it.
	const auto corpses = g_corpses.Entities(state.level);
	catalogued.insert(catalogued.end(), corpses.begin(), corpses.end());
	// Rooms are listed whether or not the game has them loaded. Exploration keeps
	// the rooms stood in and the unexplored ones at the edge of the known map.
	auto entries = g_rooms.Entries();
	if (!g_fullAssistance) {
		const auto& levelRooms = CurrentRooms(state.level, state.session);
		entries = RoomCatalogue::Exploration(entries, TerrainService::VisitedRooms(state.level, levelRooms),
			TerrainService::ExploredRooms(state.level, levelRooms, world.player), &grid);
	}
	const Strings::Table& strings = Strings::Current();
	const auto rooms = RoomCatalogue::Entities(entries, strings.surveyRoom, strings.surveyRoomUnexplored);
	catalogued.insert(catalogued.end(), rooms.begin(), rooms.end());
	// An area border is measured to where it is walked across, where the beacon
	// leads: the crossing loaded now, else one seen this game.
	if (NativeLevelExits::Layout layout; NativeEntities::ReadLayout(state.player, true, layout)) {
		const auto crossings = ZoneTransitions::Find(world, layout, Navigation::MaxRadius);
		CrossingMemory::Note(state.session, state.level, crossings);
		for (auto& entity : catalogued)
			if (!ZoneTransitions::MoveToCrossing(entity, crossings) && ZoneTransitions::Crosses(entity))
				if (const auto known = CrossingMemory::Find(state.session, state.level, entity.destinationLevel, world.player))
					entity.position = *known;
	}
	const auto death = DeathPlaceService::Landmarks(NativeDrlg::MapSeed(NativeWorld::PlayerRoom(), state.level), state.level, world.player, catalogued);
	catalogued.insert(catalogued.end(), death.begin(), death.end());
	landmarks = MapSurvey::Survey(world, grid, catalogued, g_fullAssistance);
	if (g_reachFilter) LeaveOutCutOff(world, landmarks);
	return true;
}

void Answer(const D2RL::PluginContext* context, Request request) {
	const Strings::Table& strings = Strings::Current();
	std::vector<MapSurvey::Landmark> landmarks;
	int level = 0;
	if (!CurrentSurvey(landmarks, level) || landmarks.empty()) {
		ScreenReader::Speak(strings.surveyNothing, true);
		return;
	}

	if (request == Request::PreviousCategory || request == Request::NextCategory) {
		g_category = MapSurvey::StepCategory(landmarks, g_category, request == Request::NextCategory ? 1 : -1);
		g_selected = 0;
	g_selectedPlace = 0;
	} else if (g_category == EntityRadar::Category::Unknown) {
		g_category = MapSurvey::StepCategory(landmarks, g_category, 0);
	}

	const std::vector<MapSurvey::Landmark> members = MapSurvey::OfCategory(landmarks, g_category);
	if (members.empty()) {
		ScreenReader::Speak(Format(strings.surveyEmpty, CategoryName(g_category)), true);
		return;
	}

	// A selection left alone for a while is forgotten: the next flick starts again
	// from the nearest, wherever the player has walked since (a tester).
	const uint64_t now = GetTickCount64();
	const bool flick = request == Request::NextObject || request == Request::PreviousObject;
	const bool stale = flick && g_answeredAt && now - g_answeredAt >= StaleSelectionMs;
	g_answeredAt = now;
	bool found = false;
	size_t index = 0;
	for (size_t i = 0; i < members.size() && !stale; ++i) {
		if (Selected(members[i].entity)) { index = i; found = true; break; }
	}
	if (found && request == Request::NextObject) index = (index + 1) % members.size();
	else if (found && request == Request::PreviousObject) index = (index + members.size() - 1) % members.size();
	g_selected = members[index].entity.id;
	g_selectedPlace = members[index].entity.placeId;
	// Hand the selection to the beacon, so the left-stick click leads to what
	// the scanner just named. Every answer re-offers, including a plain repeat,
	// so the offer is always the thing the player heard most recently.
	BeaconService::Offer(members[index].entity, level, GetTickCount64());
	TutorialService::Scanned(members[index].entity);

	// The category name first when the category changed, so the player knows
	// which list they are in before hearing the object itself. The name only:
	// how many it holds is a count, and counts are not spoken.
	std::string text;
	if (request == Request::PreviousCategory || request == Request::NextCategory) {
		text = CategoryName(g_category);
		text += ". ";
	}
	text += Describe(members[index]);
	// A seal says how many are left to open, the count the quest is waiting on.
	if (members[index].entity.seal) text += ". " + QuestSeals::LeftText(QuestSeals::Left(g_state.session), Strings::Current());
	AsyncLog::InfoF(context, "[survey] category=%s %zu/%zu -> %s",
		CategoryName(g_category), index + 1, members.size(), text.c_str());
	ScreenReader::Speak(text, true);
	if (members[index].entity.npcAlert) EntityCueService::PlayNpcAlert();
}

auto __cdecl OnAction(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* userData) noexcept
	-> D2RL::Input::ActionResult {
	if (event == nullptr || event->kind != D2RL::Input::ActionEventKind::Pressed) {
		return D2RL::Input::ActionResult::Ignored;
	}
	const auto request = static_cast<Request>(reinterpret_cast<intptr_t>(userData));
	if (request == Request::ReachFilter) { g_reachToggle = true; return D2RL::Input::ActionResult::Handled; }
	// A backslash typed into a box of text the keyboard's cursor is on is the box's.
	if (request == Request::Repeat && KeyboardMenu::Typing()) return D2RL::Input::ActionResult::Ignored;
	// In a menu backslash says again what the keyboard's cursor is on.
	if (request == Request::Repeat && KeyboardMenu::Active()) KeyboardMenu::Deliver(KeyboardMenu::Act::SayAgain);
	else g_request.store(request);
	return D2RL::Input::ActionResult::Handled;
}

}

// The right stick always drives the scanner during play: left/right through
// the objects in a category, up/down through categories. TakeRightStickPush
// resolves one dominant axis per push, so a slightly diagonal push cannot
// change the category and the object at once. In a menu the stick belongs to
// the menu (the user, 2026-09-21: "pathfinder should only work when menu is closed").
void PollStick() {
	// Peek before the gate: it walks the panel tree, and the stick is idle on
	// all but a few of these ticks. A push left unclaimed stays pending for the
	// reader that does want it -- in a menu, the menu's own.
	if (InputDirection::PeekRightStickPush() == InputDirection::StickPush::None || !RadarUi::InPlay()) return;
	// Character select has no menu panel, so out of a game the stick is the front end's (its row's controls).
	{
		std::lock_guard lock(g_mutex);
		if (!g_state.ready) return;
	}
	switch (InputDirection::TakeRightStickPush("scanner")) {
	case InputDirection::StickPush::Left:  g_request.store(Request::PreviousObject); break;
	case InputDirection::StickPush::Right: g_request.store(Request::NextObject); break;
	case InputDirection::StickPush::Up:    g_request.store(Request::PreviousCategory); break;
	case InputDirection::StickPush::Down:  g_request.store(Request::NextCategory); break;
	default: break;
	}
}

void Repeat() { g_request.store(Request::Repeat); }

bool ReachFilter() { return g_reachFilter.load(); }
bool TownNpcsEverywhere() { return g_townNpcsEverywhere.load(); }
void SetTownNpcsEverywhere(bool on) { g_townNpcsEverywhere = on; }
void SetReachFilter(bool on) { g_reachFilter = on; }

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context   = context;
	g_supported = NativeWorld::Initialize(context) && NativeEntities::Initialize(context);
	if (!g_supported) return false;
	// The level-area reader and the level bearings walk the act's DRLG
	// structures; a failed check switches them off and room memory keys on the layout alone.
	NativeDrlg::SetAvailable(LayoutCheck::Verify(context, context->exeBase, DrlgLayout::Checks, "rooms",
		"level areas and level bearings are off; room memory falls back to the room layout alone"));

	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize,
			.kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) return false;
		g_listeners.push_back(handle);
	}


	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		using D2RL::Input::Key;
		using D2RL::Input::Modifier;
		struct Entry { const char* id; const char* label; Key key; Modifier modifier; Request request; };
		// Ctrl+Numpad mirrors the right stick for testing without a controller.
		for (const Entry& entry : {
			Entry{"survey-repeat", "Repeat the current map object", Key::Backslash, Modifier::None, Request::Repeat},
			Entry{"survey-stick-up", "Scanner: previous category", Key::Numpad8, Modifier::Control, Request::PreviousCategory},
			Entry{"survey-stick-down", "Scanner: next category", Key::Numpad2, Modifier::Control, Request::NextCategory},
			Entry{"survey-stick-left", "Scanner: previous object", Key::Numpad4, Modifier::Control, Request::PreviousObject},
			Entry{"survey-stick-right", "Scanner: next object", Key::Numpad6, Modifier::Control, Request::NextObject},
			Entry{"survey-reach-filter", "Scanner: list only what can be reached", Key::U, Modifier::Control, Request::ReachFilter}}) {
			const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = entry.id, .displayName = entry.label, .category = "Accessibility",
				.defaultPrimary = {entry.key, entry.modifier}, .callback = OnAction,
				.userData = reinterpret_cast<void*>(static_cast<intptr_t>(entry.request))};
			D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
			if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) {
				g_actions.push_back(handle);
			}
		}
	}
	return true;
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	if (!g_supported) return;
	ApplyMode();

	if (g_reachToggle.exchange(false)) {
		const bool on = !g_reachFilter.load();
		g_reachFilter = on;
		const Strings::Table& strings = Strings::Current();
		AsyncLog::InfoF(context, "[survey] reach filter %s", on ? "on" : "off");
		ScreenReader::Speak(on ? strings.reachFilterOn : strings.reachFilterOff, true);
	}

	PollStick();

	if (const Request request = g_request.exchange(Request::None); request != Request::None && !uiOwnsSpeech) {
		// The backslash key goes through the same gate as the stick.
		if (!RadarUi::InPlay()) {
			AsyncLog::Info(context, "[survey] request ignored: not in play");
			return;
		}
		Answer(context, request);
		return;
	}

	// Keep the catalogue fed while walking, so an object seen once is still
	// known after it leaves the window. Not in a menu, where the scanner cannot
	// be asked, and less often standing still: on a slow machine one pass took
	// 25-53 ms of the game's update (a tester's log, 2026-10-12). A question
	// always reads the world afresh (CurrentSurvey).
	const uint64_t now = GetTickCount64();
	if (now < g_nextObserve) return;
	g_nextObserve = now + ObserveIntervalMs;

	Lifecycle state;
	{
		std::lock_guard lock(g_mutex);
		state = g_state;
	}
	if (!Playing(state) || !RadarUi::InPlay()) return;
	using Clock = std::chrono::steady_clock;
	const auto start = Clock::now();
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return;
	const bool still = state.level == g_observedLevel && state.session == g_observedSession
		&& std::hypot(world.player.x - g_observedAt.x, world.player.y - g_observedAt.y) < StillDistance;
	if (still && now - g_observedTime < StillObserveMs) return;
	g_observedLevel = state.level;
	g_observedSession = state.session;
	g_observedAt = world.player;
	g_observedTime = now;
	ForgetOtherGames(state);
	g_registry.Select(state.level, !g_fullAssistance);
	const auto read = Clock::now();
	Scene scene;
	if (!Collect(state, scene)) return;
	LogAreas(context, state);
	const auto collected = Clock::now();
	const auto grid = Navigation::Build(world, SurveyRadius, Radar::Layer::Movement, false);
	const auto built = Clock::now();
	Observe(state, world, grid, scene);
	// Rooms are marked explored by standing in them, so this has to keep up
	// with the player rather than wait for the next catalogue question.
	g_rooms.Observe(state.level, NativeDrlg::MapSeed(NativeWorld::PlayerRoom(), state.level),
		CurrentRooms(state.level, state.session), world.player);
	const auto done = Clock::now();
	const auto ms = [](Clock::duration span) { return std::chrono::duration<double, std::milli>(span).count(); };
	// Which part of a slow pass took the time, at most once per SlowLogMs.
	if (ms(done - start) >= SlowPassMs && now >= g_nextSlowLog) {
		g_nextSlowLog = now + SlowLogMs;
		AsyncLog::InfoF(context, "[survey] slow pass %.1f ms: world %.1f, entities %.1f, grid %.1f, catalogue %.1f",
			ms(done - start), ms(read - start), ms(collected - read), ms(built - collected), ms(done - built));
	}
}

void Shutdown() {
	if (g_context && g_input) {
		for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	}
	if (g_context && g_lifecycle) {
		for (const auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	}
	g_listeners.clear();
	g_actions.clear();
	g_registry.Clear();
	g_corpses.Clear();
	g_townNpcs.Clear();
}

}
