#include "keyboard_service.h"

#include "async_log.h"
#include "auto_detail.h"
#include "character_stats_reader.h"
#include "focus_tracker.h"
#include "auto_walk.h"
#include "battle/skill_cast.h"
#include "beacon.h"
#include "beacon_audio.h"
#include "beacon_navigation.h"
#include "beacon_service.h"
#include "combat_target.h"
#include "entity_cue_audio.h"
#include "entity_cue_service.h"
#include "entity_cues.h"
#include "entity_radar.h"
#include "entity_radar_service.h"
#include "equipment_census.h"
#include "explore_goal.h"
#include "footstep_service.h"
#include "footsteps.h"
#include "game_text_reader.h"
#include "generated/skill_attacks.h"
#include "ground_item_service.h"
#include "route_legs.h"
#include "ground_items.h"
#include "item_audio.h"
#include "loop_pace.h"
#include "grid_edge_service.h"
#include "ground_item_text.h"
#include "input_direction.h"
#include "interaction_reader.h"
#include "inventory_arrival_service.h"
#include "keyboard_menu.h"
#include "level_route.h"
#include "map_survey.h"
#include "map_survey_service.h"
#include "modal_reader.h"
#include "mod_sounds.h"
#include "native_actions.h"
#include "native_belt_fill.h"
#include "native_class_choice.h"
#include "native_list_rows.h"
#include "native_stash_pages.h"
#include "native_belt_rows.h"
#include "native_entities.h"
#include "spoken_direction.h"
#include "native_ground_items.h"
#include "native_level_exits.h"
#include "native_rooms.h"
#include "native_skill_hand.h"
#include "native_skill_icons.h"
#include "native_skill_navigation.h"
#include "native_sort.h"
#include "native_swap.h"
#include "native_value_controls.h"
#include "native_weapon_set.h"
#include "native_world.h"
#include "navigation_grid.h"
#include "pad_filter.h"
#include "pad_typing.h"
#include "percent_steps.h"
#include "navigation_mode.h"
#include "quest_service.h"
#include "radar_config.h"
#include "radar_readout.h"
#include "radar_service.h"
#include "wall_bump.h"
#include "radar_ui.h"
#include "room_graph.h"
#include "room_service.h"
#include "screen_reader.h"
#include "settings_list.h"
#include "sound_glossary.h"
#include "sound_volume.h"
#include "step_counter.h"
#include "settings_store.h"
#include "skill_bar_service.h"
#include "skill_hotkey_service.h"
#include "skill_navigation.h"
#include "speech_priority.h"
#include "speech_strings.h"
#include "terrain_service.h"
#include "audio_description.h"
#include "descriptions.h"
#include "place_service.h"
#include "boss_life_service.h"
#include "trade_service.h"
#include "tutorial_service.h"
#include "vitals.h"
#include "vitals_service.h"
#include "wall_slide_service.h"
#include "wall_tone_service.h"
#include "weapon_swap.h"
#include "weapon_swap_service.h"
#include "zone_transitions.h"

#include <D2RLPlugin/context.h>
#include <D2RLPlugin/input.h>
#include <D2RLPlugin/lifecycle_events.h>

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <map>
#include <filesystem>
#include <utility>
#include <string_view>
#include <vector>

namespace D2RAccess::KeyboardService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
using EntityRadar::Entity;
using NativeActions::Hand;
using NativeActions::Unit;
using Radar::Position;

enum class Command : uint32_t {
	None, Interact, TargetNext, TargetPrevious, ItemNext, ItemPrevious, NpcNext, NpcPrevious, WalkToTarget, Explore,
	Retreat, Stop, RoomLayout,
	ReadPrevious, ReadNext, ReadFirst, ReadLast, CompareItem, CompareEquipped, Repeat,
	// Alt+PageUp/PageDown: the shared stash's page before and after.
	StashPagePrevious, StashPageNext,
	// In a menu: the description of what the cursor is on, the pad's right stick up.
	ReadDetail,
	// Space and the right stick right on the settings list: open a group, or change
	// what the list stands on; Backspace and the stick left: back to the groups.
	Settings, SettingsChange, SettingsBack,
	// LB and RB (Shift+Space) on the settings list: a setting one value down or up.
	SettingsLower, SettingsRaise,
	// LB alone in play, with the list shut: the audible loot filter on or off (a tester, 2026-10-14).
	LootFilter,
	// In play and in a menu: the game's own key for it is W, the right hand's.
	Swap,
	// In play and in a menu: the list of skills for the left hand. The game's
	// own key, S, opens the right hand's and it has none for the left.
	LeftSkill,
	// The fighting keys are holds; their presses arrive through g_pressed.
	Attack, Cast,
	// In a menu only: a click with Shift, Ctrl or Alt held, and a press of a button of the menu.
	ClickLeft, ClickRight, Transmute, Sort, FillBelt, Repair,
	// Console only, for the first live tests.
	TestRun, TestPlain, TestHalt, TestStatus, LogOn, LogOff,
};
enum class Mode { Idle, Walk, Interact, Explore, Retreat, Fight };
enum class Progress { Going, Arrived, Blocked, NoRoute, Unreadable };

// Policy, not game constants. Distances are collision subtiles.
constexpr uint64_t ActMs = 200;       // how often a walk, a fight or a retreat looks again
constexpr uint64_t StepMs = 200;      // how often a held arrow gives its next order
constexpr uint64_t StrikeMs = 250;    // how often a held fighting key strikes again
constexpr uint64_t GearMs = 1000;     // how often the weapon in hand is looked at
constexpr double Stride = 10;         // how far ahead a held arrow aims
constexpr double UseReach = 3;        // a thing is used from this near, plus half its footprint
constexpr double UseRange = 20;       // how far D looks for something to use, in walking steps
constexpr double CrossBy = 5;         // how far past an area border's crossing a walk goes on
constexpr double MeleeReach = 3;      // a blow lands from this near
constexpr uint64_t SettingsIdleMs = 30000; // a settings list left open and unused closes itself
constexpr uint64_t SwapAnswerMs = 700; // a swap the game has not made by then was refused
constexpr uint64_t BackMs = 1500;     // how long one fall back may take before the fight goes on
constexpr double RangedReach = 24;    // a shot or a spell is loosed from this near: see docs/keyboard-actions.md

struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = 0, cameFrom = 0, revision = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
// A command typed in the console waits here until the console is closed and
// the game is in play again: the console itself is not play.
std::atomic<Command> g_deferred{Command::None};
// A key that presses a button of the open menu, by the button's name in the layout.
std::atomic<const char*> g_press{nullptr};
std::atomic<bool> g_sort{false}, g_fill{false};
// A swap that was asked for: when, and the set that was in use. 0 when none is waited for.
uint64_t g_swapAsked = 0;
bool g_swapFrom = false;
constexpr const char* RepairAllButton = "button_repair_all";
constexpr const char* RepairButton = "button_repair";
constexpr const char* TransmuteButton = "convert";
// The fighting keys, left hand then right: the key while it is held (its
// virtual-key code, 0 when up) and whether it went down since the last update.
std::atomic<uint32_t> g_held[2]{};
std::atomic<bool> g_pressed[2]{};
std::atomic<int> g_testX{0}, g_testY{0};
// Whether the game was in play at the last update. Out of play the keys that
// act on the world are left to the game, so A still opens the character screen
// from a menu and Tab is the game's own there.
std::atomic<bool> g_playing{false};
// While the settings list is open the reading keys (or the pad's right stick)
// step through it and Space changes a switch; closed, Space is the game's.
std::atomic<bool> g_settingsOpen{false};
std::atomic<bool> g_updateCheck{true}; // only saved to settings.txt, for the launcher (docs/launcher.md)
SettingsList::Menu g_settings;
Strings::Language g_settingsLanguage{};
bool g_settingsBuilt = false;
// A step past the list's end stops there with a knock instead of going round with a click.
std::atomic<bool> g_settingsStop{false};
std::atomic<bool> g_settingsRestart{false};

int g_revision = 0;
Mode g_mode = Mode::Idle;
Hand g_hand = Hand::Left;
Entity g_target;            // what a walk, a use or a retreat heads for
uint32_t g_itemPick = 0;    // the item, object or NPC comma, full stop, [ or ] chose, which the next D takes
EntityRadar::Kind g_itemPickKind = EntityRadar::Kind::Item; // an item, an object or an NPC: ids of different kinds may meet
std::string g_itemPickName;
Position g_walkFrom;        // where a walk to what the scanner named began
bool g_walkFromSet = false;
AutoWalk::Walker g_walker;
AutoWalk::Settings g_walk;
CombatTarget::Lock g_lock;
CombatTarget::Settings g_targets;
std::vector<uint64_t> g_refused; // rooms an exploring walk gave up on, this level
uint64_t g_nextAct = 0, g_nextStep = 0, g_nextStrike = 0, g_nextGear = 0;
bool g_stepping = false;    // a held arrow is walking the character
bool g_closing = false;     // a fight is walking up to its monster
bool g_backing = false;     // a ranged fight is falling back from one
bool g_struck = true;       // a blow was struck since the last fall back
Position g_backTo;
uint64_t g_backStart = 0, g_corneredLog = 0;
CombatTarget::Spacing g_spacing;
bool g_saidNone = false;    // "no enemies" was said for this hold
uint32_t g_saidOutOfReach = 0; // the monster last said to be out of reach
bool g_shooting = false;    // a bow or crossbow is in hand
RoomGraph::Graph g_graph;
int g_graphLevel = 0;
uint64_t g_graphSession = 0;

std::string Format(const char* format, auto... args) {
	char buffer[512]{};
	std::snprintf(buffer, sizeof(buffer), format, args...);
	return buffer;
}
double Distance(Position a, Position b) { return std::hypot(a.x - b.x, a.y - b.y); }
double HalfFootprint(const Entity& entity) {
	const auto side = std::max(entity.footprint.width, entity.footprint.height);
	return side > 1 ? (side - 1) / 2.0 : 0;
}
// How near a cell must be to count as reaching a thing. A shut door's collision
// is its own DOOR bit, not a footprint, and covers the cell beside its anchor:
// 2026-09-29 D said "Nothing to use here" two cells from one, and comma and
// full stop never listed one. It is reached from within its own length.
int Margin(const Entity& entity) {
	const double door = entity.doorSpan > 1 ? (entity.doorSpan - 1) / 2.0 : 0;
	return 1 + static_cast<int>(std::ceil(std::max(HalfFootprint(entity), door)));
}

const RoomGraph::Graph* Rooms(const Lifecycle& state) {
	if (g_graphLevel != state.level || g_graphSession != state.session || g_graph.Empty()) {
		std::vector<NativeRooms::Room> rooms;
		g_graph = NativeRooms::Read(NativeWorld::PlayerRoom(), rooms, state.level) ? RoomGraph::Build(rooms) : RoomGraph::Graph{};
		g_graphLevel = state.level;
		g_graphSession = state.session;
	}
	return g_graph.Empty() ? nullptr : &g_graph;
}

// What the game has shown of the level's floor this game (level_route.h), for
// the levels walked most lately: the game keeps a room's collision only while
// the player is near, and a walk that knows only what is loaded plans again
// from nothing at every room border. Null when the level's rooms cannot be
// read or are too many cells to keep.
constexpr size_t RememberedLevels = 8;
// `padThings`: each pad's entity as last read, to walk to when it is not loaded.
struct Remembered { int level = 0; uint64_t used = 0; LevelRoute::Memory memory; LevelRoute::Pads pads; std::vector<Entity> padThings; };
std::vector<Remembered> g_remembered;
uint64_t g_rememberedSession = 0, g_rememberedClock = 0;
LevelRoute::Plan g_plan;
const LevelRoute::Memory* Remember(const Lifecycle& state, const Radar::World& world) {
	if (!state.level || world.level != state.level) return nullptr;
	if (g_rememberedSession != state.session) {
		// Another game: the same level is another map.
		g_remembered.clear();
		g_plan.Clear();
		g_rememberedSession = state.session;
	}
	auto found = std::find_if(g_remembered.begin(), g_remembered.end(), [&](const Remembered& entry) { return entry.level == world.level; });
	if (found != g_remembered.end()) {
		// The game's list as it is now, not the graph cached when the level began:
		// a room added since would stay outside the map (docs/beacon.md).
		std::vector<NativeRooms::Room> rooms;
		if (NativeRooms::Read(NativeWorld::PlayerRoom(), rooms, state.level) && found->memory.Extend(rooms))
			AsyncLog::InfoF(g_context, "[route] the map grew with the level: %zu rooms, %dx%d from %d,%d",
				rooms.size(), found->memory.Width(), found->memory.Height(), found->memory.X(), found->memory.Y());
	}
	if (found == g_remembered.end()) {
		const auto* graph = Rooms(state);
		Remembered entry{world.level};
		if (!graph || !entry.memory.Begin(state.session, world.level, graph->rooms)) return nullptr;
		if (g_remembered.size() >= RememberedLevels)
			g_remembered.erase(std::min_element(g_remembered.begin(), g_remembered.end(),
				[](const Remembered& a, const Remembered& b) { return a.used < b.used; }));
		g_remembered.push_back(std::move(entry));
		found = g_remembered.end() - 1;
	}
	found->used = ++g_rememberedClock;
	(void)found->memory.Learn(world);
	return &found->memory;
}

// The teleport pads seen in a level whose map is kept, and where each leads
// (pad_links.h), for as long as the map. Null when the map is not kept.
Remembered* Kept(const Lifecycle& state) {
	if (g_rememberedSession != state.session) return nullptr;
	const auto found = std::find_if(g_remembered.begin(), g_remembered.end(), [&](const Remembered& entry) { return entry.level == state.level; });
	return found == g_remembered.end() ? nullptr : &*found;
}
LevelRoute::Pads* PadsOf(const Lifecycle& state) {
	auto* kept = Kept(state);
	return kept ? &kept->pads : nullptr;
}
LevelRoute::Pad PadOf(const Entity& entity) {
	return {static_cast<int>(std::lround(entity.position.x)), static_cast<int>(std::lround(entity.position.y)), entity.padClass};
}
// The pads among things just read are kept with the level's map.
void NotePads(const Lifecycle& state, const std::vector<Entity>& entities) {
	auto* kept = Kept(state);
	if (!kept) return;
	auto* pads = &kept->pads;
	bool added = false;
	for (const auto& entity : entities) {
		if (!entity.padClass || entity.unit == Entity::NoUnit) continue;
		added = pads->Note(PadOf(entity)) || added;
		const auto known = std::find_if(kept->padThings.begin(), kept->padThings.end(), [&](const Entity& thing) { return PadOf(thing) == PadOf(entity); });
		if (known == kept->padThings.end()) kept->padThings.push_back(entity);
		else *known = entity;
	}
	if (!added) return;
	const auto* graph = Rooms(state);
	const auto links = graph ? pads->Links(graph->rooms) : std::vector<LevelRoute::Link>{};
	AsyncLog::InfoF(g_context, "[keyboard] pads: level %d, %zu known, %zu with a destination, %zu of them seen used", state.level,
		pads->All().size(), links.size(), static_cast<size_t>(std::count_if(links.begin(), links.end(), [](const auto& link) { return link.seen; })));
}

// Whether the game has shown that a thing cannot be got to on foot from where
// the player stands (LevelRoute::Reach). The ground reached is found again only
// when the map changes or the player is on other ground, as past a teleport
// pad: it is a search of the whole level.
LevelRoute::Reach g_reach;
const LevelRoute::Reach* Reached(const Lifecycle& state, const Radar::World& world) {
	const auto* memory = Remember(state, world);
	if (!memory) return nullptr;
	if (!g_reach.Holds(*memory, world.player, world.bodyPattern)) g_reach = LevelRoute::Reach(*memory, world.player, world.bodyPattern);
	return &g_reach;
}
bool CutOff(const LevelRoute::Reach& reach, const Lifecycle& state, const Entity& target) {
	using Answer = LevelRoute::Reach::Answer;
	if (target.kind == EntityRadar::Kind::Room) {
		// A room is walked into, not up to: any of its ground will do.
		const auto* graph = Rooms(state);
		if (!graph) return false;
		const auto room = std::find_if(graph->rooms.begin(), graph->rooms.end(), [&](const auto& known) { return known.id == target.placeId; });
		return room != graph->rooms.end() && reach.Within(NativeRooms::Subtiles(*room)) == Answer::CutOff;
	}
	// As near as a walk counts as there (Walk: UseReach and half the footprint),
	// which is wider than the margin a route is planned to: stairs set into a
	// wall have no place for the body beside them, and are walked up to all the same.
	const int within = std::max(Margin(target), static_cast<int>(std::ceil(UseReach + HalfFootprint(target))));
	return reach.To(target.position, within) == Answer::CutOff;
}
bool CutOff(const Lifecycle& state, const Radar::World& world, const Entity& target) {
	const auto* reach = Reached(state, world);
	return reach && CutOff(*reach, state, target);
}

// The ground got to through the teleport pads as well (LevelRoute::Reach with
// the pads' links), searched only in a level with a pad whose way is known, and
// asked only of what walking alone does not get to. 2026-10-02, the Arcane
// Sanctuary: H said "Nothing left to explore here" fourteen times with fourteen
// rooms not explored, every one of them past a pad.
LevelRoute::Reach g_padReach;
const LevelRoute::Reach* PadReached(const Lifecycle& state, const Radar::World& world) {
	const auto* memory = Remember(state, world);
	auto* pads = PadsOf(state);
	if (!memory || !pads || pads->All().empty()) return nullptr;
	if (!g_padReach.Holds(*memory, world.player, world.bodyPattern, pads->Revision())) {
		const auto* graph = Rooms(state);
		const auto links = graph ? pads->Links(graph->rooms) : std::vector<LevelRoute::Link>{};
		if (links.empty()) return nullptr;
		// A pad is used from as near as D uses one (UseSteps); a pad has no footprint.
		g_padReach = LevelRoute::Reach(*memory, world.player, world.bodyPattern, links, static_cast<int>(std::ceil(UseReach)), pads->Revision());
	}
	return &g_padReach;
}
// A thing got to through the pads, and the pad to use first on the way. False
// as well when walking alone gets to it: then no pad is wanted.
bool PadWayFrom(const LevelRoute::Reach& reach, const Lifecycle& state, const Entity& target, LevelRoute::Pad& pad) {
	if (target.kind == EntityRadar::Kind::Room) {
		const auto* graph = Rooms(state);
		if (!graph) return false;
		const auto room = std::find_if(graph->rooms.begin(), graph->rooms.end(), [&](const auto& known) { return known.id == target.placeId; });
		return room != graph->rooms.end() && reach.ViaWithin(NativeRooms::Subtiles(*room), pad);
	}
	// As near as a walk counts as there, as CutOff has it.
	const int within = std::max(Margin(target), static_cast<int>(std::ceil(UseReach + HalfFootprint(target))));
	return reach.Via(target.position, within, pad);
}
bool PadWay(const Lifecycle& state, const Radar::World& world, const Entity& target, LevelRoute::Pad& pad) {
	const auto* reach = PadReached(state, world);
	return reach && PadWayFrom(*reach, state, target, pad);
}
// The pad to walk to: as loaded now, else as last read.
std::optional<Entity> PadThing(const Lifecycle& state, const std::vector<Entity>& loaded, const LevelRoute::Pad& pad) {
	for (const auto& entity : loaded)
		if (entity.padClass && entity.unit != Entity::NoUnit && PadOf(entity) == pad) return entity;
	if (const auto* kept = Kept(state))
		for (const auto& thing : kept->padThings)
			if (PadOf(thing) == pad) return thing;
	return std::nullopt;
}

// What is around the player right now. Read fresh for every decision: the
// monsters move, and so has the player.
struct Scene {
	Radar::World world;
	Navigation::Grid grid; // the terrain layer: a monster in a doorway is not a wall
	std::vector<Entity> entities;
};
bool Look(const Lifecycle& state, Scene& scene, bool exits, bool wholeLevel = false) {
	if (!NativeWorld::Read(state.player, state.session, state.level, scene.world)) return false;
	scene.grid = Navigation::Build(scene.world, Navigation::MaxRadius, Radar::Layer::Terrain, false);
	if (!NativeEntities::Read(state.player, state.session, scene.entities, exits, exits, wholeLevel)) scene.entities.clear();
	NotePads(state, scene.entities);
	// An area border is walked across where both levels' floor meets.
	if (NativeLevelExits::Layout layout; exits && NativeEntities::ReadLayout(state.player, true, layout)) {
		const auto crossings = ZoneTransitions::Find(scene.world, layout, Navigation::MaxRadius);
		for (auto& entity : scene.entities) ZoneTransitions::MoveToCrossing(entity, crossings);
	}
	return true;
}

// Walking steps to a thing: its own cells carry its collision, so the count is
// the fewest steps to any cell within `margin` of it. -1 when none is reached.
int Steps(const Navigation::Grid& grid, Position position, int margin) {
	int fewest = -1;
	const int x = static_cast<int>(std::lround(position.x)), y = static_cast<int>(std::lround(position.y));
	for (int dy = -margin; dy <= margin; ++dy) {
		for (int dx = -margin; dx <= margin; ++dx) {
			const auto* cell = grid.At(x + dx, y + dy);
			if (cell && cell->steps >= 0 && (fewest < 0 || cell->steps < fewest)) fewest = cell->steps;
		}
	}
	return fewest;
}
// The steps to where D can use a thing from. A teleport pad at a walkway's end
// stands on a cell the game marks shut, the drop beyond it, so no cell beside
// it holds the body: 2026-10-02 09:37:58 at 25145,5387 full stop said "Nothing
// to use here" five times, and none of the seven pads standing so was ever used,
// though each is the way back across its gap. Such a pad counts from as near as
// a walk counts as there (Walk: UseReach and half the footprint).
int UseSteps(const Navigation::Grid& grid, const Entity& entity) {
	const int steps = Steps(grid, entity.position, Margin(entity));
	if (steps >= 0 || !entity.padClass) return steps;
	return Steps(grid, entity.position, static_cast<int>(std::ceil(UseReach + HalfFootprint(entity))));
}

// A thing by name, distance and direction, as the scanner says it.
std::string Describe(const Radar::World& world, const Entity& entity, int steps) {
	const auto& strings = Strings::Current();
	const Position offset{entity.position.x - world.player.x, entity.position.y - world.player.y};
	const char* direction = SpokenDirection::Of(offset, strings);
	const double reach = steps >= 0 ? static_cast<double>(steps) : Distance(world.player, entity.position);
	const auto distance = EntityRadar::DistancePhrase(reach, EntityRadar::Settings{}, strings);
	return Format(strings.surveySteps, EntityRadar::SpokenName(entity, strings.listSeparator).c_str(), distance.c_str(), direction);
}

void CancelMovement(bool halt) noexcept {
	g_mode = Mode::Idle;
	g_walker.Stop();
	g_closing = g_backing = false;
	if (halt) (void)NativeActions::Halt();
}

void End(const char* reason, bool halt) {
	if (g_mode != Mode::Idle) AsyncLog::InfoF(g_context, "[keyboard] ended: %s", reason);
	CancelMovement(halt);
}

bool Ready() {
	if (NativeActions::Available()) return true;
	ScreenReader::Speak(Strings::Current().actionsUnavailable, true);
	return false;
}

// Log only, no change to the walk: an order the character did not move on is
// given again unchanged, and the game's own flags of each cell on the straight
// line to it are logged, to find what the game will not walk that our map
// calls open (2026-09-28: 5791,4784 refused three times from 5807,4783 in the camp).
Position g_orderFrom, g_lastOrder;
bool g_ordered = false;
void LogStall(const Radar::World& world, Position order) {
	const double length = Distance(world.player, order);
	std::string cells;
	int lastX = 0, lastY = 0, listed = 0;
	for (double along = 0; length >= 1 && along <= length && listed < 24; along += 0.5) {
		const int x = static_cast<int>(std::lround(world.player.x + (order.x - world.player.x) * along / length));
		const int y = static_cast<int>(std::lround(world.player.y + (order.y - world.player.y) * along / length));
		if (listed && x == lastX && y == lastY) continue;
		lastX = x;
		lastY = y;
		++listed;
		uint16_t flags = 0;
		const auto state = world.Cell(x, y, Radar::Layer::Movement, &flags);
		char cell[40];
		std::snprintf(cell, sizeof(cell), " %d,%d=%04X/%d", x, y, flags, static_cast<int>(state));
		cells += cell;
	}
	AsyncLog::InfoF(g_context, "[keyboard] walk: no move on %.0f,%.0f from %.0f,%.0f, mask %04X; on the way:%s",
		order.x, order.y, world.player.x, world.player.y, world.walkingMask, cells.c_str());
}

// Every order to walk goes through here. One given again from where it was
// first given, more than a second on, the character not having moved, is
// logged once a place with the character's own state and the game's cells
// around it: 2026-09-28 the character stood at 7580,13196 for 90 seconds and
// the game refused every order, the arrows' and the mouse's, and the log
// could not say why (mode, path, or the floor under it).
constexpr uint64_t StuckMs = 1000;
Position g_moveOrder{-1, -1}, g_moveFrom{-1, -1}, g_stuckAt{-1, -1};
uint64_t g_moveSince = 0;
void LogStuck(const Radar::World& world, Position order) {
	const auto state = NativeActions::ReadMoveState();
	std::string rows;
	const int px = static_cast<int>(world.player.x), py = static_cast<int>(world.player.y);
	for (int dy = -2; dy <= 2; ++dy) {
		if (dy > -2) rows += " |";
		for (int dx = -2; dx <= 2; ++dx) {
			uint16_t flags = 0;
			const auto cell = world.Cell(px + dx, py + dy, Radar::Layer::Movement, &flags);
			char text[16];
			std::snprintf(text, sizeof(text), " %04X/%d", flags, static_cast<int>(cell));
			rows += text;
		}
	}
	AsyncLog::InfoF(g_context, "[keyboard] stuck: no move for %llu ms at %d,%d, ordered to %.0f,%.0f; read=%u mode=%u path_type=%u"
		" position_raw=%08X,%08X pattern=%d mask %04X; cells x-2..x+2, rows y-2..y+2:%s",
		static_cast<unsigned long long>(StuckMs), px, py, order.x, order.y, state.read ? 1u : 0u, state.mode, state.pathType,
		state.x, state.y, world.bodyPattern, world.walkingMask, rows.c_str());
}
// Markers left on the game's own map where no unit stands any more
// (radar.h LeftMarkers) are taken off it, a cell at a time, with the game's
// own single-cell reset. 2026-09-29, the Blood Moor: two player stamps beside
// the character and a shrine on the third side refused every first step until
// Save and Exit. 2026-09-30, the Catacombs: three more a cell ahead of the
// character, each with an arm or two already gone, so the first remedy, which
// wanted a whole cross, took none of them off; a marker with nothing round it
// at 22570,9623; and a doorway walked through at 11:48:00 that no route
// passed eighteen seconds later. The user, 2026-10-01: the mod leaves them,
// something in how it orders the character to move; which call is not measured.
//
// Away from the character they are taken off twice a second in play. On the
// eight cells round it only once the game has refused a move, or the arrows
// find no step: a live stamp can sit a cell off the character, and only a
// refusal shows that one there is in the way. The reset reaches a cell only
// through the character's room and the rooms beside it, so a marker farther
// off does not come off yet: that is said once, and it is tried again when the
// character has come some way from where it was tried.
constexpr uint64_t SweepMs = 500;
constexpr double RetryAfter = 20; // cells walked before a marker that stayed is tried again
uint64_t g_nextSweep = 0, g_nextBeside = 0;
struct Kept { int x = 0, y = 0; Position from; }; // would not come off, tried from there
std::vector<Kept> g_kept;
void TakeOffMarkers(const Lifecycle& state, const Radar::World& world, bool beside) {
	const auto known = [](const Radar::LeftMarker& marker) {
		return std::find_if(g_kept.begin(), g_kept.end(), [&](const Kept& other) { return other.x == marker.x && other.y == marker.y; });
	};
	const auto kept = [&](const Radar::LeftMarker& marker) {
		const auto found = known(marker);
		return found != g_kept.end() && Distance(world.player, found->from) < RetryAfter;
	};
	// The cells are looked at first, with no unit known: nearly always there is
	// nothing, and the units of the whole level are not read for nothing.
	std::vector<Position> units;
	auto left = Radar::LeftMarkers(world, units, beside);
	std::erase_if(left, kept);
	if (left.empty()) return;
	std::vector<Entity> entities;
	// Every loaded room of the level: without the units a live one's marker cannot be told from a left one.
	if (!NativeEntities::Read(state.player, state.session, entities, false, false, true)) return;
	for (const auto& entity : entities)
		if (entity.unit == static_cast<uint8_t>(Unit::Player) || entity.unit == static_cast<uint8_t>(Unit::Monster)) units.push_back(entity.position);
	left = Radar::LeftMarkers(world, units, beside);
	std::erase_if(left, kept);
	// With other players in the game, or a roster that cannot be read, a
	// stamp away from the character may be a player's whose unit was not read:
	// only a bare marker is taken off then, and a stamp after a refused move.
	std::vector<CorpseMemory::Owner> roster;
	const bool alone = NativeEntities::ReadRosterCorpses(state.player, roster)
		&& std::none_of(roster.begin(), roster.end(), [&](const CorpseMemory::Owner& owner) { return owner.id != state.player; });
	if (!alone && !beside) std::erase_if(left, [](const Radar::LeftMarker& marker) { return (marker.flags & 0x80) != 0; });
	if (left.empty()) return;
	for (const auto& marker : left)
		for (const auto& cell : Radar::MarkerCells(world, marker)) (void)NativeWorld::ClearCell(cell.x, cell.y, cell.mask);
	// The map is read again: only that shows what came off.
	Radar::World after;
	const bool read = NativeWorld::Read(state.player, state.session, state.level, after);
	for (const auto& marker : left) {
		uint16_t now = marker.flags;
		const bool gone = read && after.Cell(marker.x, marker.y, Radar::Layer::Movement, &now) != Radar::State::Unknown && !(now & 0x1000);
		const auto before = known(marker);
		// One that stayed is said once; when it comes off at a later try, that is said too.
		if (gone || before == g_kept.end())
			AsyncLog::InfoF(g_context, "[keyboard] marker left at %d,%d (%04X, %s), %.0f cells from the character%s: %s",
				marker.x, marker.y, marker.flags, marker.flags & 0x80 ? "a player stamp" : "bare",
				Distance(world.player, {static_cast<double>(marker.x), static_cast<double>(marker.y)}),
				beside ? ", looked for after a step the character could not take" : "", gone ? "taken off" : "NOT taken off");
		if (gone) { if (before != g_kept.end()) g_kept.erase(before); }
		else if (before != g_kept.end()) before->from = world.player;
		else g_kept.push_back({marker.x, marker.y, world.player});
	}
}
// The way the player came into the level, which Ctrl+R walks back to: where
// they first stood in it, and what they came through -- stairs, a waypoint, a
// portal, an area border -- which stands beside that. Looked for at the first
// look in the level, and again while the player is still there and nothing
// has been found. With nothing found the place itself is the way in, so a
// retreat always has somewhere to go.
//
// A hop within the level, a teleport pad of the Arcane Sanctuary, can put the
// player on ground the way in cannot be walked to from (2026-10-02 08:44: the
// waypoint cut off from 25378,5446, past the pads at 25385 and 25396). The
// way onto that ground is the pad hopped onto, kept the same way.
constexpr double ArriveNear = 10; // what the player came through stands this near where they first stood
constexpr double HopNear = 3;     // a pad hopped onto is under the player: a thing no farther off than this
constexpr double HopFar = 20;     // farther than this between two looks is a hop, not a stride...
constexpr uint64_t HopMs = 1000;  // ...when the looks are no farther apart than this
struct Arrival { Position at; Entity way; bool named = false; };
struct WayIn { uint64_t session = 0; int level = 0; Arrival entered, hopped; bool hop = false; Position last; uint64_t lastAt = 0; };
WayIn g_wayIn;
// What the player came through, beside `arrival`: the nearest `kind` within `within`.
void Name(const Lifecycle& state, Arrival& arrival, EntityRadar::Kind kind, double within, const char* what) {
	Scene scene;
	if (!Look(state, scene, true)) return;
	// The level left beats what merely stands near: the stairs back up, not a waypoint beside them.
	const auto back = [&](const Entity& entity) { return state.cameFrom && static_cast<int>(entity.destinationLevel) == state.cameFrom; };
	const Entity* way = nullptr;
	for (const auto& entity : scene.entities) {
		if ((entity.kind != kind && entity.kind != EntityRadar::Kind::Exit) || Distance(entity.position, arrival.at) > within) continue;
		if (!way || back(entity) > back(*way)
			|| (back(entity) == back(*way) && Distance(entity.position, arrival.at) < Distance(way->position, arrival.at))) way = &entity;
	}
	if (!way) return;
	arrival.way = *way;
	arrival.named = true;
	AsyncLog::InfoF(g_context, "[keyboard] %s: %s at %.0f,%.0f, %.0f cells from where the player came in at %.0f,%.0f (level %d, from %d)",
		what, way->name.c_str(), way->position.x, way->position.y, Distance(way->position, arrival.at), arrival.at.x, arrival.at.y, state.level, state.cameFrom);
}
void NoteWayIn(const Lifecycle& state, const Radar::World& world, uint64_t now) {
	if (world.level != state.level) return;
	if (g_wayIn.session != state.session || g_wayIn.level != state.level) {
		g_wayIn = {state.session, state.level, {world.player}};
	} else if (now - g_wayIn.lastAt <= HopMs && Distance(world.player, g_wayIn.last) > HopFar) {
		AsyncLog::InfoF(g_context, "[keyboard] hop from %.0f,%.0f to %.0f,%.0f", g_wayIn.last.x, g_wayIn.last.y, world.player.x, world.player.y);
		g_wayIn.hopped = {world.player};
		g_wayIn.hop = true;
	}
	g_wayIn.last = world.player;
	g_wayIn.lastAt = now;
	if (!g_wayIn.entered.named && Distance(world.player, g_wayIn.entered.at) <= ArriveNear)
		Name(state, g_wayIn.entered, EntityRadar::Kind::Exit, ArriveNear, "way in");
	if (g_wayIn.hop && !g_wayIn.hopped.named && Distance(world.player, g_wayIn.hopped.at) <= HopNear)
		Name(state, g_wayIn.hopped, EntityRadar::Kind::Object, HopNear, "way in, hopped onto");
}

// A teleport pad D used: the player's being sent is said, and where the pad
// put them is checked against where it was worked out to lead (pad_links.h),
// which is the measure of that working out. 2026-10-02 a pad sent the player
// 0.25 to 0.6 s after D, and the pads of a pair stood 6 cells apart at the
// least, across a void.
constexpr uint64_t SentWithinMs = 2000; // longer after D is no teleport
constexpr uint64_t SentGapMs = 250;     // two reads no farther apart than this...
constexpr double SentFar = 4;           // ...and farther apart than this are a pad's jump: running covers 1.4 cells in 0.1 s
struct PadUse { bool active = false; uint64_t session = 0, since = 0, lastAt = 0; int level = 0; LevelRoute::Pad pad; Position last; };
PadUse g_padUse;
void WatchPad(const Lifecycle& state, const Entity& pad, Position player, uint64_t now) {
	g_padUse = {true, state.session, now, now, state.level, PadOf(pad), player};
}
void Sent(const Lifecycle& state, uint64_t now) {
	if (!g_padUse.active) return;
	const auto& pad = g_padUse.pad;
	if (state.session != g_padUse.session || state.level != g_padUse.level || now - g_padUse.since > SentWithinMs) {
		g_padUse.active = false;
		AsyncLog::InfoF(g_context, "[keyboard] pad at %d,%d class %u used, and no teleport seen", pad.x, pad.y, pad.padClass);
		return;
	}
	const auto move = NativeActions::ReadMoveState();
	if (!move.read) return;
	const Position at{move.x / 65536.0, move.y / 65536.0};
	const bool jumped = now - g_padUse.lastAt <= SentGapMs && Distance(at, g_padUse.last) > SentFar;
	g_padUse.last = at;
	g_padUse.lastAt = now;
	if (!jumped) return;
	g_padUse.active = false;
	ScreenReader::Speak(Strings::Current().teleported, true);
	auto* pads = PadsOf(state);
	const auto* graph = Rooms(state);
	std::optional<LevelRoute::Link> expected;
	LevelRoute::Pad to;
	bool beside = false;
	if (pads) {
		if (graph) expected = pads->Lead(pad, graph->rooms);
		beside = pads->Used(pad, at, to);
	}
	char landed[64] = "beside no pad known", lead[96] = "nowhere known";
	if (beside) std::snprintf(landed, sizeof landed, "beside the pad at %d,%d", to.x, to.y);
	if (expected) std::snprintf(lead, sizeof lead, "the pad at %d,%d, %s", expected->to.x, expected->to.y, expected->seen ? "seen before" : "by the rule");
	AsyncLog::InfoF(g_context, "[keyboard] teleported: pad at %d,%d class %u, landed %.0f,%.0f %s; expected %s: %s",
		pad.x, pad.y, pad.padClass, at.x, at.y, landed, lead,
		!expected || !beside ? "nothing to compare" : expected->to == to ? "as expected" : "NOT as expected");
}

void Sweep(const Lifecycle& state, uint64_t now) {
	if (now < g_nextSweep) return;
	g_nextSweep = now + SweepMs;
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return;
	NoteWayIn(state, world, now);
	// The floor is remembered here too, twice a second in play: what the arrows
	// walk past is known to a later walk.
	(void)Remember(state, world);
	TakeOffMarkers(state, world, false);
}
// The order is "refused" while it is given again and again from the same
// cell: a second tap of the same arrow, seconds after the first, is not that.
// A walk gives a far order again only when it has stalled (AutoWalk stallMs,
// 1200), so the gap that still counts as "again and again" is longer than that.
constexpr uint64_t OrderGapMs = 2000;
static_assert(OrderGapMs > AutoWalk::Settings{}.stallMs, "a stalled walk's repeated order must still count as the same refusal");
uint64_t g_moveLast = 0;
void Order(const Lifecycle& state, const Radar::World& world, Position order, uint64_t now) {
	if (order.x != g_moveOrder.x || order.y != g_moveOrder.y || Distance(world.player, g_moveFrom) >= 1
		|| now - g_moveLast > OrderGapMs) {
		g_moveOrder = order;
		g_moveFrom = world.player;
		g_moveSince = now;
	} else if (now - g_moveSince >= StuckMs) {
		// Logged once a place; the markers are looked for again every second it lasts.
		if (Distance(world.player, g_stuckAt) >= 1) {
			g_stuckAt = world.player;
			LogStuck(world, order);
		}
		g_moveSince = now;
		TakeOffMarkers(state, world, true);
	}
	g_moveLast = now;
	(void)NativeActions::MoveTo(order);
}

// The way to a thing the loaded floor does not reach, over what the game has
// shown of the level so far: `ahead` is as far along it as the loaded floor
// goes. False when the remembered map has no way either.
//
// 2026-10-01, the Far Oasis, the walk to the Maggot Lair: from the rooms by
// the lair the loaded floor reached it, 230-odd steps back south and round a
// dune, and the walk turned south; two rooms south the lair's surroundings
// were freed, the room graph's guess led north again, into the dead end under
// the dune, and so on from 13:03 to 13:07 (orders north to y 4355, then south
// to y 4402, over and over). The room graph still leads when the map has no way.
constexpr uint64_t LedLogMs = 3000;     // how often the log says where the map leads
constexpr uint64_t LedPlaceMs = 1000;   // a thing that moves is led to where it was, this long at most
constexpr double LedPlaceDrift = 10;    // and no farther off than this: each new place is a search of the level
uint64_t g_ledLog = 0, g_ledPlaceAt = 0;
bool g_ledGuess = false;
Position g_ledPlace{-1, -1};
bool Led(const Lifecycle& state, const Scene& scene, const Entity& target, int margin, uint64_t now, Position& ahead) {
	const auto* memory = Remember(state, scene.world);
	if (!memory) return false;
	if (now < g_ledPlaceAt || now - g_ledPlaceAt >= LedPlaceMs || Distance(target.position, g_ledPlace) > LedPlaceDrift) {
		g_ledPlace = target.position;
		g_ledPlaceAt = now;
	}
	const auto way = g_plan.From(*memory, scene.world.player, g_ledPlace, margin, scene.world.bodyPattern);
	if (!LevelRoute::Ahead(way.path, scene.grid, ahead)) return false;
	if (now < g_ledLog || now - g_ledLog >= LedLogMs || way.guess != g_ledGuess) {
		g_ledLog = now;
		g_ledGuess = way.guess;
		AsyncLog::InfoF(g_context, "[keyboard] walk: led by the remembered map to %s at %.0f,%.0f: %zu cells from %.0f,%.0f, %s; now to %.0f,%.0f",
			target.name.c_str(), target.position.x, target.position.y, way.path.size(), scene.world.player.x, scene.world.player.y,
			way.guess ? "part of it ground not seen yet" : "all of it seen", ahead.x, ahead.y);
	}
	return true;
}

// One leg of a walk toward a thing, along the route the beacon would ping.
Progress Advance(const Lifecycle& state, const Scene& scene, const Entity& target, double reach, uint64_t now) {
	if (Distance(scene.world.player, target.position) <= reach) return Progress::Arrived;
	const int margin = Margin(target);
	Beacon::Settings settings;
	settings.arrivalDistance = reach;
	// The scanner keeps terrain reachability. A retreat must also steer round
	// monsters standing on that route, using the same route finder on live collision.
	if (g_mode == Mode::Retreat) settings.layer = Radar::Layer::Movement;
	Position destination = target.position;
	const char* toward = "the thing itself";
	Beacon::Route route;
	if (Steps(scene.grid, target.position, margin) < 0 && Led(state, scene, target, margin, now, destination)) {
		toward = "the way over the remembered map";
		route = Beacon::FindRoute(scene.world, destination, settings);
	} else {
		const auto guidance = BeaconNavigation::Prepare(scene.world, target.position, Rooms(state), nullptr, margin, &scene.grid);
		if (!guidance.available) return Progress::Unreadable;
		if (guidance.hasWaypoint) {
			destination = guidance.waypoint;
			toward = "the way into the next room";
		} else {
			destination = target.position;
		}
		route = Beacon::FindRoute(guidance.world, destination, settings);
	}
	if (route.path.empty()) {
		// No cell the character can reach is nearer the place than its own.
		AsyncLog::InfoF(g_context, "[keyboard] walk: no route from %.0f,%.0f to %.0f,%.0f (%s) for %s at %.0f,%.0f",
			scene.world.player.x, scene.world.player.y, destination.x, destination.y,
			toward, target.name.c_str(), target.position.x, target.position.y);
		return Progress::NoRoute;
	}
	auto walking = g_walk;
	walking.layer = settings.layer;
	const auto step = g_walker.Tick(scene.world, route.path, now, walking);
	if (step.event == AutoWalk::Event::Blocked) return Progress::Blocked;
	if (step.event == AutoWalk::Event::Order) {
		if (g_ordered && step.order.x == g_lastOrder.x && step.order.y == g_lastOrder.y && Distance(scene.world.player, g_orderFrom) < 1)
			LogStall(scene.world, step.order);
		g_ordered = true;
		g_lastOrder = step.order;
		g_orderFrom = scene.world.player;
		Order(state, scene.world, step.order, now);
	}
	return Progress::Going;
}

// No way on foot to a thing further off than `reach`: the loaded floor does not
// get to it, and the remembered map shows it cut off.
bool NoWay(const Lifecycle& state, const Scene& scene, const Entity& target, double reach) {
	if (Distance(scene.world.player, target.position) <= reach) return false;
	if (target.kind != EntityRadar::Kind::Room && Steps(scene.grid, target.position, Margin(target)) >= 0) return false;
	return CutOff(state, scene.world, target);
}

// ---- walking to what the scanner named, and to the nearest thing to use ----

// A walk to `target`, a retreat's too: false when it is not begun. The whole
// way is looked for before the first step. 2026-10-02, the Arcane Sanctuary: a
// walk to a teleport pad across the void ran for four seconds, to eight cells
// from it, and said "No way" there (07:10:06 to 07:10:10).
//
// Where only teleport pads get to it, the walk goes to the first pad of the way
// and stops there, for D to use: "the player should still have to stop and
// interact with it with d. but it's fine if the route has to split into legs
// accounting for each pad" (the user, 2026-10-02). The walk key again goes on
// from where the pad put the player.
bool SetOut(const Lifecycle& state, const Entity& target, Mode mode, const char* reason) {
	Entity leg = target;
	if (Scene scene; Look(state, scene, false) && NoWay(state, scene, target, UseReach + HalfFootprint(target))) {
		LevelRoute::Pad pad;
		std::optional<Entity> first;
		if (PadWay(state, scene.world, target, pad)) first = PadThing(state, scene.entities, pad);
		if (!first) {
			End("no way", true);
			AsyncLog::InfoF(g_context, "[keyboard] walk: not begun, no way on foot from %.0f,%.0f to %s at %.0f,%.0f",
				scene.world.player.x, scene.world.player.y, target.name.c_str(), target.position.x, target.position.y);
			ScreenReader::Speak(Format(Strings::Current().walkNoRoute, target.name.c_str()), true);
			return false;
		}
		leg = *first;
		AsyncLog::InfoF(g_context, "[keyboard] walk: by teleport pad at %d,%d, the first of the way to %s at %.0f,%.0f",
			pad.x, pad.y, target.name.c_str(), target.position.x, target.position.y);
	}
	End(reason, false);
	g_target = leg;
	g_mode = mode;
	g_walker.Start();
	g_nextAct = 0;
	g_walkFromSet = false;
	return true;
}

void BeginWalk(const Lifecycle& state) {
	const auto& strings = Strings::Current();
	if (!Ready()) return;
	Entity target;
	if (!BeaconService::Offered(target, state.level)) { ScreenReader::Speak(strings.walkNothing, true); return; }
	if (!SetOut(state, target, Mode::Walk, "walk to target")) return;
	AsyncLog::InfoF(g_context, "[keyboard] walk to id=%u at=%.0f,%.0f name=%s", target.id, target.position.x, target.position.y, target.name.c_str());
	ScreenReader::Speak(Format(strings.walkTo, target.name.c_str()), true);
}

// Where a thing that is not there is gone: close enough that it would be loaded,
// as the scanner forgets it (MapSurvey::Settings::forgetWithin). Further off, what
// the scanner still lists is walked to where it was (the user, 2026-09-28:
// "Target lost" on a remembered shrine 90 steps away, 27 ms into the walk).
bool Near(const Radar::World& world, Position position) {
	return Distance(world.player, position) <= MapSurvey::Settings{}.forgetWithin;
}

// The thing as it is now: a unit moves, and is a unit only while its room is
// loaded, under an id that is new each time its room is loaded again.
bool Refresh(const Lifecycle& state, const Scene& scene, Entity& target) {
	if (target.kind == EntityRadar::Kind::Room) return true;
	if (target.kind == EntityRadar::Kind::Item) {
		GroundItems::Snapshot snapshot;
		// As far as the scanner reads items: what it named may be rooms away.
		if (!NativeGroundItems::ReadLevel(state.player, state.session, snapshot)) return true; // unread is not gone
		for (const auto& item : snapshot.items)
			if (item.id == target.id) { target.position = {item.position.x, item.position.y}; return true; }
		// One of the same name where it lay serves as well.
		for (const auto& item : snapshot.items)
			if (item.source == GroundItems::SourceKind::Item && item.name == target.name
				&& std::abs(item.position.x - target.position.x) <= 1 && std::abs(item.position.y - target.position.y) <= 1) {
				target.id = item.id;
				target.position = {item.position.x, item.position.y};
				return true;
			}
		return !Near(scene.world, target.position);
	}
	for (const auto& entity : scene.entities)
		if (EntityRadar::SameTarget(entity, target)) { target = entity; return true; }
	for (const auto& entity : scene.entities)
		if (EntityRadar::Recreated(target, entity)) { target = entity; return true; }
	// Not loaded is not gone: a place keeps where it was, and a unit too until
	// the player is close enough to see it is not there.
	return target.placeId != 0 || target.unit == Entity::NoUnit || !Near(scene.world, target.position);
}

bool InTargetRoom(const Lifecycle& state, Position player) {
	const auto* graph = Rooms(state);
	const size_t room = graph ? NativeRooms::Containing(graph->rooms, player) : NativeRooms::None;
	return room != NativeRooms::None && graph->rooms[room].id == g_target.placeId;
}

// A point past an area border's crossing, the way the walk came to it. The
// crossing is the last cell on this side: the walk stopped there and two steps
// more changed the level (MEASURED 2026-09-28, the Blood Moor from the camp).
Position Beyond(Position player, Position crossing) {
	Position way{crossing.x - player.x, crossing.y - player.y};
	if (std::hypot(way.x, way.y) < 1) way = {crossing.x - g_walkFrom.x, crossing.y - g_walkFrom.y};
	const double length = std::hypot(way.x, way.y);
	if (length < 1) return crossing;
	return {std::round(crossing.x + way.x / length * CrossBy), std::round(crossing.y + way.y / length * CrossBy)};
}

void Walk(const Lifecycle& state, uint64_t now) {
	const auto& strings = Strings::Current();
	Scene scene;
	// A walk to what the scanner named, and a retreat, look as far as the scanner does.
	if (!Look(state, scene, g_target.kind == EntityRadar::Kind::Exit, g_mode == Mode::Walk || g_mode == Mode::Retreat)) return;
	const std::string name = g_target.name;
	if (!Refresh(state, scene, g_target)) {
		if (g_mode != Mode::Retreat) {
			End("target gone", true);
			ScreenReader::Speak(Format(strings.beaconLost, name.c_str()), true);
			return;
		}
		// A portal closes: where it stood is still the way in.
		AsyncLog::InfoF(g_context, "[keyboard] retreat: %s is gone, on to where it was, %.0f,%.0f", name.c_str(), g_target.position.x, g_target.position.y);
		g_target.unit = Entity::NoUnit;
	}
	if (!g_walkFromSet) { g_walkFrom = scene.world.player; g_walkFromSet = true; }
	const double reach = UseReach + HalfFootprint(g_target);
	if (g_mode == Mode::Explore && InTargetRoom(state, scene.world.player)) {
		End("explored", true);
		ScreenReader::Speak(strings.exploreArrived, true);
		return;
	}
	// Cut off, as the map now shows: said here, not from the edge nearest the thing.
	if (g_mode != Mode::Interact && NoWay(state, scene, g_target, g_mode == Mode::Explore ? 0 : reach)) {
		AsyncLog::InfoF(g_context, "[keyboard] walk: no way on foot from %.0f,%.0f to %s at %.0f,%.0f",
			scene.world.player.x, scene.world.player.y, name.c_str(), g_target.position.x, g_target.position.y);
		End("no way", true);
		ScreenReader::Speak(Format(strings.walkNoRoute, name.c_str()), true);
		return;
	}
	switch (Advance(state, scene, g_target, g_mode == Mode::Explore ? 0 : reach, now)) {
	case Progress::Arrived:
		if (g_mode == Mode::Interact) {
			AsyncLog::InfoF(g_context, "[keyboard] use id=%u unit=%u name=%s", g_target.id, g_target.unit, name.c_str());
			End("used", false);
			if (g_target.padClass) WatchPad(state, g_target, scene.world.player, now);
			(void)NativeActions::Interact(static_cast<Unit>(g_target.unit), g_target.id);
		} else if (g_mode == Mode::Walk && g_target.kind == EntityRadar::Kind::Exit) {
			// An exit is taken, not stood beside: stairs or a doorway are used as
			// D uses them; an area border is walked on across.
			End("arrived", false);
			ScreenReader::Speak(Format(strings.beaconArrived, name.c_str()), true);
			if (g_target.unit != Entity::NoUnit) {
				AsyncLog::InfoF(g_context, "[keyboard] take exit id=%u unit=%u name=%s", g_target.id, g_target.unit, name.c_str());
				(void)NativeActions::Interact(static_cast<Unit>(g_target.unit), g_target.id);
			} else {
				const Position beyond = Beyond(scene.world.player, g_target.position);
				AsyncLog::InfoF(g_context, "[keyboard] cross into %s: on to %.0f,%.0f", name.c_str(), beyond.x, beyond.y);
				(void)NativeActions::MoveTo(beyond);
			}
		} else {
			End("arrived", true);
			ScreenReader::Speak(Format(strings.beaconArrived, name.c_str()), true);
			// At a teleport pad the next D uses it, as if comma or full stop had
			// chosen it, and not whatever else stands nearer.
			if (g_target.padClass && g_target.unit != Entity::NoUnit) {
				g_itemPick = g_target.id;
				g_itemPickKind = g_target.kind;
				g_itemPickName = name;
			}
		}
		break;
	case Progress::Blocked:
		if (g_mode == Mode::Explore) {
			// This room cannot be got into from here; the next ask tries another.
			g_refused.push_back(g_target.placeId);
			AsyncLog::InfoF(g_context, "[keyboard] explore: gave up on room %llu", static_cast<unsigned long long>(g_target.placeId));
		}
		End("blocked", true);
		ScreenReader::Speak(Format(strings.walkBlocked, name.c_str()), true);
		break;
	case Progress::NoRoute:
		End("no route", true);
		ScreenReader::Speak(Format(strings.walkNoRoute, name.c_str()), true);
		break;
	case Progress::Going:
	case Progress::Unreadable:
		break;
	}
}

// ---- choosing what D uses ----

// Comma and full stop: the items on the ground on screen, nearest on foot
// first. The one chosen is what the next D picks up, in place of whatever is
// nearest (the user, 2026-09-28: "I can't specifically target items I want").
// Every object counts here too -- chests, urns, dead bodies, shrines, levers,
// doors, the stash -- and a player's corpse (the user, 2026-09-28), on screen
// as Tab's monsters are. [ and ] step through the NPCs on screen the same way,
// for the next D to talk to the one chosen when other things stand closer
// (the user, 2026-09-28).
void StepPick(const Lifecycle& state, int delta, bool npcs) {
	const auto& strings = Strings::Current();
	const std::string none = npcs ? Format(strings.surveyEmpty, strings.surveyCategories[3]) : strings.interactNothing;
	Scene scene;
	GroundItems::Snapshot snapshot;
	if (!Look(state, scene, false)) {
		ScreenReader::Speak(none, true);
		return;
	}
	if (npcs || !NativeGroundItems::Read(state.player, state.session, snapshot)) snapshot.items.clear();
	struct Pick { Entity entity; int steps = 0; };
	std::vector<Pick> picks;
	for (const auto& item : snapshot.items) {
		if (item.source != GroundItems::SourceKind::Item || item.name.empty()) continue;
		Entity entity;
		entity.id = item.id;
		entity.kind = EntityRadar::Kind::Item;
		entity.category = EntityRadar::Category::Item;
		entity.position = {item.position.x, item.position.y};
		entity.name = item.name;
		entity.unit = static_cast<uint8_t>(Unit::Item);
		if (!CombatTarget::OnScreen(scene.world.player, entity.position)) continue;
		const int steps = Steps(scene.grid, entity.position, Margin(entity));
		if (steps >= 0) picks.push_back({std::move(entity), steps});
	}
	for (const auto& entity : scene.entities) {
		const bool usable = npcs ? entity.kind == EntityRadar::Kind::Npc
			: entity.kind == EntityRadar::Kind::Object || entity.kind == EntityRadar::Kind::Corpse;
		if (!usable || entity.unit == Entity::NoUnit || !CombatTarget::OnScreen(scene.world.player, entity.position)) continue;
		const int steps = UseSteps(scene.grid, entity);
		if (steps >= 0) picks.push_back({entity, steps});
	}
	std::stable_sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) {
		return a.steps != b.steps ? a.steps < b.steps : a.entity.id < b.entity.id;
	});
	if (picks.empty()) {
		g_itemPick = 0;
		ScreenReader::Speak(none, true);
		return;
	}
	size_t index = delta > 0 ? 0 : picks.size() - 1;
	for (size_t i = 0; i < picks.size(); ++i)
		if (picks[i].entity.id == g_itemPick && picks[i].entity.kind == g_itemPickKind) { index = (i + picks.size() + (delta > 0 ? 1 : picks.size() - 1)) % picks.size(); break; }
	const auto& pick = picks[index];
	g_itemPick = pick.entity.id;
	g_itemPickKind = pick.entity.kind;
	g_itemPickName = pick.entity.name;
	AsyncLog::InfoF(g_context, "[keyboard] item chosen id=%u kind=%d steps=%d name=%s", pick.entity.id, static_cast<int>(pick.entity.kind),
		pick.steps, pick.entity.name.c_str());
	ScreenReader::Speak(Describe(scene.world, pick.entity, pick.steps), true);
}

void BeginUse(const Lifecycle& state) {
	const auto& strings = Strings::Current();
	if (!Ready()) return;
	Scene scene;
	if (!Look(state, scene, false)) { ScreenReader::Speak(strings.interactNothing, true); return; }
	std::vector<Entity> things;
	for (const auto& entity : scene.entities) {
		using EntityRadar::Kind;
		if (entity.unit == Entity::NoUnit) continue;
		if (entity.kind == Kind::Npc || entity.kind == Kind::Object || entity.kind == Kind::Exit || entity.kind == Kind::Corpse)
			things.push_back(entity);
	}
	GroundItems::Snapshot snapshot;
	if (NativeGroundItems::Read(state.player, state.session, snapshot)) {
		for (const auto& item : snapshot.items) {
			if (item.source != GroundItems::SourceKind::Item) continue;
			Entity entity;
			entity.id = item.id;
			entity.kind = EntityRadar::Kind::Item;
			entity.category = EntityRadar::Category::Item;
			entity.position = {item.position.x, item.position.y};
			entity.name = item.name;
			entity.unit = static_cast<uint8_t>(Unit::Item);
			things.push_back(std::move(entity));
		}
	}
	const Entity* nearest = nullptr;
	int fewest = 0;
	bool chosen = false;
	// The item comma or full stop chose is taken, once, in place of whatever is
	// nearest. One that has gone takes nothing else in its place.
	if (const uint32_t pick = std::exchange(g_itemPick, 0)) {
		for (const auto& thing : things) {
			if (thing.kind != g_itemPickKind || thing.id != pick) continue;
			const int steps = UseSteps(scene.grid, thing);
			if (steps >= 0 && CombatTarget::OnScreen(scene.world.player, thing.position)) { nearest = &thing; fewest = steps; chosen = true; }
			break;
		}
		if (!chosen) {
			AsyncLog::InfoF(g_context, "[keyboard] use: the item chosen, id=%u, is gone", pick);
			ScreenReader::Speak(Format(strings.beaconLost, g_itemPickName.c_str()), true);
			return;
		}
	}
	for (const auto& thing : things) {
		if (chosen) break;
		const int steps = UseSteps(scene.grid, thing);
		if (steps < 0 || steps > UseRange || (nearest && !EntityRadar::UseBefore(thing, steps, *nearest, fewest))) continue;
		if (!CombatTarget::OnScreen(scene.world.player, thing.position)) continue; // only what could be clicked
		nearest = &thing;
		fewest = steps;
	}
	if (!nearest) { ScreenReader::Speak(strings.interactNothing, true); return; }
	End("use", false);
	g_target = *nearest;
	g_mode = Mode::Interact;
	g_walker.Start();
	g_nextAct = 0;
	AsyncLog::InfoF(g_context, "[keyboard] use: nearest id=%u unit=%u steps=%d name=%s", g_target.id, g_target.unit, fewest, g_target.name.c_str());
	ScreenReader::Speak(g_target.name, true);
}

// ---- exploring ----

// No room left that walking alone gets to: the nearest past the teleport pads,
// by the steps of the way, is walked toward as a walk to what the scanner named
// is (SetOut): to the first pad of the way, for D to use. False when the pads
// get to none.
bool ExploreByPad(const Lifecycle& state, const Scene& scene, const RoomGraph::Graph& graph, const std::vector<NativeRooms::Room>& explored) {
	const auto* walking = Reached(state, scene.world);
	const auto* pads = walking ? PadReached(state, scene.world) : nullptr;
	if (!pads) return false;
	size_t best = NativeRooms::None;
	int fewest = -1;
	LevelRoute::Pad first;
	for (size_t i = 0; i < graph.rooms.size(); ++i) {
		const auto& room = graph.rooms[i];
		const auto bounds = NativeRooms::Subtiles(room);
		if (std::any_of(explored.begin(), explored.end(), [&](const auto& known) { return known.id == room.id; })
			|| std::find(g_refused.begin(), g_refused.end(), room.id) != g_refused.end()
			|| walking->Within(bounds) != LevelRoute::Reach::Answer::CutOff) continue;
		LevelRoute::Pad pad;
		int steps = 0;
		if (pads->ViaWithin(bounds, pad, &steps) && (fewest < 0 || steps < fewest)) { best = i; fewest = steps; first = pad; }
	}
	if (best == NativeRooms::None) return false;
	const auto thing = PadThing(state, scene.entities, first);
	if (!thing) return false;
	End("explore", false);
	g_target = *thing;
	g_mode = Mode::Walk;
	g_walker.Start();
	g_nextAct = 0;
	g_walkFromSet = false;
	AsyncLog::InfoF(g_context, "[keyboard] explore: room %zu by teleport pad at %d,%d, %d steps", best, first.x, first.y, fewest);
	ScreenReader::Speak(Strings::Current().exploreTo, true);
	return true;
}

void BeginExplore(const Lifecycle& state) {
	const auto& strings = Strings::Current();
	if (!Ready()) return;
	Scene scene;
	const auto* graph = Rooms(state);
	if (!graph || !Look(state, scene, false)) { ScreenReader::Speak(strings.terrainUnavailable, true); return; }
	const auto explored = TerrainService::ExploredRooms(state.level, graph->rooms, scene.world.player);
	// A room cut off from here is no place to walk to: 2026-10-02, the Arcane
	// Sanctuary, H ended three times with no way to the room it chose. Looked
	// for at each ask, since a teleport pad changes it.
	std::vector<uint64_t> refused = g_refused;
	if (const auto* reach = Reached(state, scene.world))
		for (const auto& room : graph->rooms)
			if (reach->Within(NativeRooms::Subtiles(room)) == LevelRoute::Reach::Answer::CutOff) refused.push_back(room.id);
	const size_t cutOff = refused.size() - g_refused.size();
	const auto choice = ExploreGoal::Nearest(*graph, scene.grid, explored, scene.world.player, refused);
	if (!choice.found && cutOff && ExploreByPad(state, scene, *graph, explored)) return;
	if (!choice.found) {
		AsyncLog::InfoF(g_context, "[keyboard] explore: nothing left, %zu rooms, %zu explored, %zu given up, %zu cut off", graph->rooms.size(), explored.size(), g_refused.size(), cutOff);
		ScreenReader::Speak(strings.exploreNothing, true);
		return;
	}
	End("explore", false);
	g_target = {};
	g_target.id = static_cast<uint32_t>(choice.room + 1);
	g_target.kind = EntityRadar::Kind::Room;
	g_target.category = EntityRadar::Category::UnexploredRoom;
	g_target.position = choice.position;
	g_target.placeId = graph->rooms[choice.room].id;
	g_target.name = strings.surveyCategories[12];
	g_mode = Mode::Explore;
	g_walker.Start();
	g_nextAct = 0;
	AsyncLog::InfoF(g_context, "[keyboard] explore: room %zu at=%.0f,%.0f hops=%d", choice.room, choice.position.x, choice.position.y, choice.hops);
	ScreenReader::Speak(strings.exploreTo, true);
}

// ---- retreating ----

// Ctrl+R: back to the way the player came into the level, walked as a walk to
// what the scanner named is walked, and no further: room to drink and heal,
// not a way out of the fight worked out from the monsters. 2026-10-02, the
// Arcane Sanctuary, come into by its waypoint: Ctrl+R said "No way out found"
// and Shift+R "The way back is not known" with the waypoint on the scanner's
// list, and Alt+R said "Retreating to passage" where the player stood. The
// user: "collapse retreat to control+r autowalks immediately to the way the
// player entered from. everything else is dangerous and failure prone."
constexpr uint32_t PlaceOnly = 0xFFFFFFFF; // the id of a way in that is a place alone: no unit has it
// An arrival as a place to walk to.
Entity Target(const Arrival& arrival) {
	Entity target = arrival.way;
	if (arrival.named && target.unit == Entity::NoUnit) {
		// An area border: back to where it was crossed, ground the player has
		// walked, not to wherever along it the scanner would put it from here.
		target.id = PlaceOnly;
		target.placeId = 0;
	}
	if (!arrival.named) {
		target = {};
		target.id = PlaceOnly;
		target.kind = EntityRadar::Kind::Exit;
		target.category = EntityRadar::Category::Exit;
		target.position = arrival.at;
		target.name = Strings::Current().retreatEntrance;
	}
	return target;
}
void BeginRetreat(const Lifecycle& state) {
	const auto& strings = Strings::Current();
	if (!Ready()) return;
	if (g_wayIn.session != state.session || g_wayIn.level != state.level) {
		// Not looked at yet in this level: the world could not be read.
		ScreenReader::Speak(strings.terrainUnavailable, true);
		return;
	}
	Entity target = Target(g_wayIn.entered);
	const char* how = g_wayIn.entered.named ? "what the player came in by" : "where the player came in, nothing found there";
	// Past a hop the way in may be cut off on foot, with no pad known to lead
	// back: then the way onto this ground is the way back.
	LevelRoute::Pad pad;
	if (Scene scene; g_wayIn.hop && Look(state, scene, false) && NoWay(state, scene, target, UseReach + HalfFootprint(target))
		&& !PadWay(state, scene.world, target, pad)) {
		AsyncLog::InfoF(g_context, "[keyboard] retreat: no way on foot from %.0f,%.0f to %s at %.0f,%.0f, the way in; to the way onto this ground",
			scene.world.player.x, scene.world.player.y, target.name.c_str(), target.position.x, target.position.y);
		target = Target(g_wayIn.hopped);
		how = g_wayIn.hopped.named ? "what the player hopped onto" : "where the player hopped onto, nothing found there";
	}
	if (!SetOut(state, target, Mode::Retreat, "retreat")) return;
	AsyncLog::InfoF(g_context, "[keyboard] retreat to %s at %.0f,%.0f, %s", target.name.c_str(), target.position.x, target.position.y, how);
	ScreenReader::Speak(Format(strings.retreatTo, target.name.c_str()), true);
}

// ---- fighting ----

// Whether the weapon in hand shoots, looked at once a second.
bool Shooting(const D2RL::PluginContext* context, uint64_t now) {
	if (now < g_nextGear) return g_shooting;
	g_nextGear = now + GearMs;
	std::vector<Vitals::Breakage::Item> worn;
	if (EquipmentCensus::Read(context, worn) != EquipmentCensus::Result::Read) return g_shooting;
	g_shooting = false;
	for (const auto& item : worn)
		if ((item.bodyLocation == WeaponSwap::RightHand || item.bodyLocation == WeaponSwap::LeftHand)
			&& Generated::ShootingWeapon(NativeGroundItems::ClassCode(item.classId))) g_shooting = true;
	return g_shooting;
}

Generated::SkillAttack KindOf(Hand hand) {
	const int skill = NativeActions::SelectedSkill(hand);
	// A skill that cannot be read is fought with as the plain Attack would be.
	return skill < 0 ? Generated::SkillAttack::Weapon : Generated::SkillAttackFor(skill);
}

CombatTarget::Reach ReachOf(const D2RL::PluginContext* context, Hand hand, uint64_t now) {
	const auto kind = KindOf(hand);
	const bool ranged = kind == Generated::SkillAttack::Ranged
		|| (kind == Generated::SkillAttack::Weapon && Shooting(context, now));
	return {ranged, ranged ? RangedReach : MeleeReach};
}

std::vector<CombatTarget::Candidate> Monsters(const Scene& scene) {
	auto ranked = CombatTarget::Rank(scene.world, scene.grid, scene.entities, g_targets);
	// As with a mouse: a monster off the screen cannot be clicked, so it is not fought.
	std::erase_if(ranked, [&](const CombatTarget::Candidate& monster) {
		return !CombatTarget::OnScreen(scene.world.player, monster.entity.position);
	});
	return ranked;
}

void StepTarget(const Lifecycle& state, int delta) {
	const auto& strings = Strings::Current();
	Scene scene;
	if (!Look(state, scene, false)) { ScreenReader::Speak(strings.targetNone, true); return; }
	const auto ranked = Monsters(scene);
	const auto* target = g_lock.Choose(ranked, delta);
	if (!target) { ScreenReader::Speak(strings.targetNone, true); return; }
	const auto& enemy = target->entity.enemy;
	char slots[3 * std::tuple_size_v<decltype(enemy.enchantments)> + 1]{};
	for (size_t i = 0; i < enemy.enchantments.size(); ++i)
		std::snprintf(slots + 3 * i, sizeof(slots) - 3 * i, "%s%02X", i ? " " : "", enemy.enchantments[i]);
	AsyncLog::InfoF(g_context, "[keyboard] target id=%u steps=%d sight=%u of %zu name=%s flags=%04X slots=%s plate=%s", target->entity.id,
		target->steps, target->sight ? 1u : 0u, ranked.size(), target->entity.name.c_str(), enemy.flags, slots, enemy.plate.c_str());
	ScreenReader::Speak(Describe(scene.world, target->entity, target->steps), true);
}

void BeginFight(const D2RL::PluginContext* context, const Lifecycle& state, Hand hand, uint64_t now) {
	if (!Ready()) return;
	const int skill = NativeActions::SelectedSkill(hand);
	if (KindOf(hand) == Generated::SkillAttack::None) {
		// Not an attack: a cry, an armour, a summons. Used once. A self buff needs
		// neither terrain nor an enemy; the game owns town, mana and form rules.
		End("one-shot skill", true);
		AsyncLog::InfoF(g_context, "[keyboard] skill %d is not an attack: used once", skill);
		if (Generated::SkillRequiresEnemy(skill)) {
			// Some non-attacks (Blood Oath) still need a hostile unit.
			Scene scene;
			bool moved = false;
			if (Look(state, scene, false)) {
				const auto ranked = Monsters(scene);
				if (const auto* target = g_lock.Keep(ranked, moved)) {
					(void)NativeActions::SkillOn(hand, static_cast<Unit>(target->entity.unit), target->entity.id);
					return;
				}
			}
			ScreenReader::Speak(Strings::Current().targetNone, true);
		} else (void)NativeActions::SkillHere(hand);
		return;
	}
	End("fight", false);
	g_hand = hand;
	g_mode = Mode::Fight;
	g_walker.Start();
	g_saidNone = false;
	g_saidOutOfReach = 0;
	g_struck = true;
	g_nextAct = g_nextStrike = 0;
	const auto reach = ReachOf(context, hand, now);
	AsyncLog::InfoF(g_context, "[keyboard] fight: hand=%s skill=%d ranged=%u reach=%.0f", hand == Hand::Left ? "left" : "right",
		skill, reach.ranged ? 1u : 0u, reach.range);
}

void Fight(const D2RL::PluginContext* context, const Lifecycle& state, uint64_t now) {
	const auto& strings = Strings::Current();
	// Shift is the game's stand-still: polled here so it can change during a hold.
	const bool stationary = g_hand == Hand::Left && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
	const auto missing = SkillCast::MissingEnemy(NativeActions::SelectedSkill(g_hand), stationary);
	auto untargeted = [&] {
		if (g_closing || g_backing) { g_closing = g_backing = false; (void)NativeActions::Halt(); }
		if (missing == SkillCast::WithoutEnemy::Report) {
			if (!g_saidNone) ScreenReader::Speak(strings.targetNone, true);
			g_saidNone = true;
		} else if (now >= g_nextStrike) {
			g_nextStrike = now + StrikeMs;
			(void)NativeActions::SkillHere(g_hand);
			// Optional-target spells cast once; a stationary hold keeps striking.
			if (missing == SkillCast::WithoutEnemy::CastHere) End("untargeted skill", false);
		}
	};
	Scene scene;
	if (!Look(state, scene, false)) {
		if (missing != SkillCast::WithoutEnemy::Report) untargeted();
		return;
	}
	const auto ranked = Monsters(scene);
	bool moved = false;
	const auto* target = g_lock.Keep(ranked, moved);
	if (!target) {
		untargeted();
		return;
	}
	g_saidNone = false;
	if (moved) {
		// A monster is named once, when the fight turns to it.
		AsyncLog::InfoF(g_context, "[keyboard] fight: now id=%u name=%s", target->entity.id, target->entity.name.c_str());
		ScreenReader::Speak(target->entity.name, false);
		g_walker.Start();
	}
	if (stationary) {
		// Before chase and retreat; the stand-still command does not walk up either.
		if (g_closing || g_backing) { g_closing = g_backing = false; (void)NativeActions::Halt(); }
		if (now >= g_nextStrike) {
			g_nextStrike = now + StrikeMs;
			(void)NativeActions::SkillOn(g_hand, static_cast<Unit>(target->entity.unit), target->entity.id, true);
		}
		return;
	}
	const auto reach = ReachOf(context, g_hand, now);
	if (reach.ranged) {
		// A ranged fight keeps its distance: a shot, a fall back when a monster
		// closes, the next shot from there. The order is given again until the
		// character is free to move, since a shot under way cannot be broken off.
		if (g_backing) {
			if (Distance(scene.world.player, g_backTo) > 2 && now - g_backStart < BackMs) {
				Order(state, scene.world, g_backTo, now);
				return;
			}
			g_backing = false;
		}
		std::vector<Position> threats;
		for (const auto& monster : ranked) threats.push_back(monster.entity.position);
		if (g_struck && CombatTarget::Closing(scene.world.player, threats, g_spacing)) {
			const double here = CombatTarget::Clearance(scene.world.player, threats);
			const auto ground = CombatTarget::FallBack(scene.world, scene.grid, threats, target->entity.position, reach.range, g_spacing);
			if (ground.found) {
				AsyncLog::InfoF(g_context, "[keyboard] fight: falling back to %.0f,%.0f, nearest monster %.1f -> %.1f",
					ground.cell.x, ground.cell.y, here, ground.clearance);
				g_backing = true;
				g_struck = g_closing = false;
				g_backTo = ground.cell;
				g_backStart = now;
				g_walker.Start();
				Order(state, scene.world, g_backTo, now);
				return;
			}
			if (now - g_corneredLog >= 2000) {
				g_corneredLog = now;
				AsyncLog::InfoF(g_context, "[keyboard] fight: nowhere to fall back to with a monster at %.1f; standing", here);
			}
			// Asked again only after the next blow, not five times a second.
			g_struck = false;
		}
	}
	switch (CombatTarget::Decide(*target, reach)) {
	case CombatTarget::Move::Strike:
		if (g_closing) { g_closing = false; g_walker.Start(); }
		if (now >= g_nextStrike) {
			g_nextStrike = now + StrikeMs;
			g_struck = true;
			(void)NativeActions::SkillOn(g_hand, static_cast<Unit>(target->entity.unit), target->entity.id);
		}
		break;
	case CombatTarget::Move::Close: {
		Beacon::Settings settings;
		settings.arrivalDistance = reach.range;
		const auto route = Beacon::FindRoute(scene.world, target->entity.position, settings);
		const auto step = route.path.empty() ? AutoWalk::Step{AutoWalk::Event::Blocked, {}}
			: g_walker.Tick(scene.world, route.path, now, g_walk);
		if (step.event == AutoWalk::Event::Order) {
			g_closing = true;
			Order(state, scene.world, step.order, now);
		} else if (step.event == AutoWalk::Event::Blocked && g_saidOutOfReach != target->entity.id) {
			g_saidOutOfReach = target->entity.id;
			ScreenReader::Speak(Format(strings.targetOutOfReach, target->entity.name.c_str()), false);
			g_walker.Start();
		}
		break;
	}
	case CombatTarget::Move::Unreachable:
		if (g_saidOutOfReach != target->entity.id) {
			g_saidOutOfReach = target->entity.id;
			ScreenReader::Speak(Format(strings.targetOutOfReach, target->entity.name.c_str()), false);
		}
		break;
	}
}

// ---- the arrows ----

bool Down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }

// The world direction the held arrows ask for; zero when none is held. Each
// arrow is a screen direction, which the game's projection turns into a world
// diagonal (radar.h).
Position Heading() {
	Position heading{};
	const int keys[] = {VK_UP, VK_LEFT, VK_RIGHT, VK_DOWN}; // Radar::Direction order
	for (size_t i = 0; i < Radar::DirectionCount; ++i)
		if (Down(keys[i])) { heading.x += Radar::Axes[i].x; heading.y += Radar::Axes[i].y; }
	const double length = std::hypot(heading.x, heading.y);
	return length > 0.01 ? Position{heading.x / length, heading.y / length} : Position{};
}

void Step(const Lifecycle& state, Position heading, uint64_t now) {
	if (now < g_nextStep) return;
	g_nextStep = now + StepMs;
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return;
	// As far that way as the body walks straight, so a wall stops the walk at the wall.
	for (double reach = Stride; reach >= 1; reach -= 1) {
		const Position aim{std::round(world.player.x + heading.x * reach), std::round(world.player.y + heading.y * reach)};
		if (Distance(aim, world.player) < 1 || !AutoWalk::Straight(world, world.player, aim)) continue;
		g_stepping = true;
		Order(state, world, aim, now);
		return;
	}
	// No step that way at all: a marker left beside the character is one of
	// the things that does it, and no order is given here for the game to refuse.
	if (now >= g_nextBeside) {
		g_nextBeside = now + StuckMs;
		TakeOffMarkers(state, world, true);
	}
}

// ---- reading keys: the pad's right stick, and its left stick under Y ----

void Read(Command command, bool inPlay) {
	using InputDirection::StickPush;
	switch (command) {
	// In play the stick's left and right step through objects and its up and
	// down through categories; in a menu up and down step through what is read.
	case Command::ReadPrevious: InputDirection::PostRightStickPush(inPlay ? StickPush::Left : StickPush::Up); break;
	case Command::ReadNext:     InputDirection::PostRightStickPush(inPlay ? StickPush::Right : StickPush::Down); break;
	case Command::ReadFirst:    InputDirection::PostRightStickPush(inPlay ? StickPush::Up : StickPush::Left); break;
	case Command::ReadLast:     InputDirection::PostRightStickPush(inPlay ? StickPush::Down : StickPush::Right); break;
	case Command::CompareItem:     InputDirection::PostLeftStickPush(StickPush::Left); break;
	case Command::CompareEquipped: InputDirection::PostLeftStickPush(StickPush::Right); break;
	case Command::RoomLayout:   InputDirection::PostBackWith(InputDirection::FaceButton::Y); break;
	case Command::Repeat:
		if (inPlay) MapSurveyService::Repeat();
		else GameTextReader::RequestRepeat();
		break;
	default: break;
	}
}

// ---- the weapon swap ----

// The game's own key handler is run; the weapons in hand are named by the
// weapon swap's own announcement when the set changes (weapon_swap_service.h).
void Swap(const D2RL::PluginContext* context, uint64_t now) {
	const auto set = NativeWeaponSet::SecondSet();
	if (NativeSwap::Swap(context) != NativeSwap::Result::Asked) {
		ScreenReader::Speak(Strings::Current().unavailable, true);
		return;
	}
	if (!set) return;
	g_swapAsked = now;
	g_swapFrom = *set;
}

// The game refuses in silence: in the middle of a blow, with input taken by
// something else, and whatever the server refuses. The player is told.
void SwapAnswer(const D2RL::PluginContext* context, uint64_t now) {
	if (!g_swapAsked) return;
	const auto set = NativeWeaponSet::SecondSet();
	if (set && *set != g_swapFrom) { g_swapAsked = 0; return; }
	if (now - g_swapAsked < SwapAnswerMs) return;
	g_swapAsked = 0;
	AsyncLog::InfoF(context, "[swap] the set in use did not change in %llu ms: refused", static_cast<unsigned long long>(SwapAnswerMs));
	ScreenReader::Speak(Strings::Current().unavailable, true);
}

// ---- the settings list ----

// Pan and pitch strength steps: a quarter of the compiled 100 from none to
// RadarConfig::MaxStrengthPercent (the tester's request, 2026-10-13).
constexpr int StrengthStepPercent = PercentSteps::Step;
constexpr int StrengthLevels = static_cast<int>(RadarConfig::MaxStrengthPercent) / StrengthStepPercent + 1;
constexpr int StrengthPercent(int level) { return std::clamp(level, 0, StrengthLevels - 1) * StrengthStepPercent; }
static_assert(RadarConfig::MaxStrengthPercent % StrengthStepPercent == 0);

// A strength of a sound family as a setting of several values: `get` and `set`
// trade the scale the audio module uses (1 = as compiled).
SettingsList::Entry Strength(const char* key, const char* Strings::Table::* format,
	std::function<double()> get, std::function<void(double)> set) {
	SettingsList::Entry entry;
	entry.key = key;
	entry.levels = StrengthLevels;
	entry.level = [get] {
		const double percent = get() * RadarConfig::NeutralStrengthPercent;
		return std::clamp(static_cast<int>(std::lround(percent / StrengthStepPercent)), 0, StrengthLevels - 1);
	};
	entry.setLevel = [set](int level) { set(static_cast<double>(StrengthPercent(level)) / RadarConfig::NeutralStrengthPercent); };
	entry.sayLevel = [format](int level) {
		char phrase[160]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().*format, StrengthPercent(level));
		return std::string(phrase);
	};
	return entry;
}

// Saved as a step: the keys change with the steps (-25 = quarters), so a step of
// an older grid is not read as one of these.
std::vector<SettingsList::Entry> Strengths() {
	return {
		Strength("enemy-pan-25", &Strings::Table::enemyPanStrength, EntityCueAudio::PanScale,
			[](double scale) { EntityCueAudio::SetStrength(scale, EntityCueAudio::PitchScale()); }),
		Strength("enemy-pitch-25", &Strings::Table::enemyPitchStrength, EntityCueAudio::PitchScale,
			[](double scale) { EntityCueAudio::SetStrength(EntityCueAudio::PanScale(), scale); }),
		Strength("beacon-pan-25", &Strings::Table::beaconPanStrength, BeaconAudio::PanScale,
			[](double scale) { BeaconAudio::SetStrength(scale, BeaconAudio::PitchScale()); }),
		Strength("beacon-pitch-25", &Strings::Table::beaconPitchStrength, BeaconAudio::PitchScale,
			[](double scale) { BeaconAudio::SetStrength(BeaconAudio::PanScale(), scale); }),
		// "-half": 100 became half the old spread, so an old step is not read as one of these.
		Strength("item-pan-half-25", &Strings::Table::itemPanStrength, ItemAudio::PanScale, ItemAudio::SetPanScale),
	};
}

SettingsList::Entry ManaAlerts() {
	static_assert(Strings::ManaAlertWords == Vitals::ManaAlertLevels, "a phrase for every mana alert level");
	SettingsList::Entry entry;
	entry.key = "mana-alerts";
	entry.levels = Vitals::ManaAlertLevels;
	entry.level = VitalsService::ManaAlerts;
	entry.setLevel = VitalsService::SetManaAlerts;
	entry.sayLevel = [](int level) { return std::string(Strings::Current().manaAlerts[static_cast<size_t>(level)]); };
	return entry;
}

// Compass points or clock hours for every spoken direction (spoken_direction.h).
SettingsList::Entry DirectionStyle() {
	SettingsList::Entry entry;
	entry.key = "direction-style";
	entry.levels = static_cast<int>(SpokenDirection::Style::Count);
	entry.level = [] { return static_cast<int>(SpokenDirection::CurrentStyle()); };
	entry.setLevel = [](int level) { SpokenDirection::SetStyle(static_cast<SpokenDirection::Style>(level)); };
	entry.sayLevel = [](int level) { return std::string(Strings::Current().directionStyles[static_cast<size_t>(level)]); };
	return entry;
}

// How many units each click of the walk counts (a tester, 2026-10-20).
SettingsList::Entry StepClickInterval() {
	SettingsList::Entry entry;
	entry.key = "step-click-interval";
	entry.levels = static_cast<int>(std::size(StepCounter::Intervals));
	entry.level = GroundItemService::StepInterval;
	entry.setLevel = GroundItemService::SetStepInterval;
	entry.sayLevel = [](int level) {
		char phrase[160]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().stepClickInterval, StepCounter::Intervals[static_cast<size_t>(level)]);
		return std::string(phrase);
	};
	return entry;
}

SettingsList::Entry ItemSoundSet() {
	static_assert(Strings::ItemSoundSetCount == static_cast<size_t>(GroundItems::SoundSet::Count), "a phrase for every item sound set");
	SettingsList::Entry entry;
	entry.key = "item-sound-set";
	entry.levels = static_cast<int>(GroundItems::SoundSet::Count);
	entry.level = GroundItems::ItemSoundSet;
	entry.setLevel = GroundItems::SetItemSoundSet;
	entry.sayLevel = [](int level) { return std::string(Strings::Current().itemSoundSets[static_cast<size_t>(level)]); };
	return entry;
}

// A family's volume (sound_volume.h), from silent to double its configured level.
SettingsList::Entry Volume(const char* key, const char* Strings::Table::* format, SoundVolume::Family family) {
	SettingsList::Entry entry;
	entry.key = key;
	entry.levels = static_cast<int>(SoundVolume::Percents.size());
	entry.level = [family] { return SoundVolume::Of(family).Get(); };
	entry.setLevel = [family](int level) { SoundVolume::Of(family).Set(level); };
	entry.sayLevel = [format](int level) {
		char phrase[160]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().*format, SoundVolume::Percents[static_cast<size_t>(level)]);
		return std::string(phrase);
	};
	return entry;
}

// How fast one kind of pulsing loop repeats (the user, 2026-10-16).
SettingsList::Entry LoopSpeed(const char* key, LoopPace::Group group) {
	SettingsList::Entry entry;
	entry.key = key;
	entry.levels = static_cast<int>(LoopPace::Percents.size());
	entry.level = [group] { return LoopPace::Setting(group).load(); };
	entry.setLevel = [group](int level) { LoopPace::Setting(group) = std::clamp(level, 0, static_cast<int>(LoopPace::Percents.size()) - 1); };
	entry.sayLevel = [group](int level) {
		char phrase[200]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().loopSpeed, Strings::Current().loopPaceNames[static_cast<size_t>(group)],
			LoopPace::Percents[static_cast<size_t>(level)]);
		return std::string(phrase);
	};
	return entry;
}

// How often the radar's wall ahead repeats while the player heads into it (a tester, 2026-10-15).
SettingsList::Entry RadarWallRepeat() {
	SettingsList::Entry entry;
	entry.key = "radar-wall-repeat";
	entry.levels = static_cast<int>(std::size(Radar::WallRepeatMs));
	entry.level = RadarService::WallRepeatLevel;
	entry.setLevel = RadarService::SetWallRepeatLevel;
	entry.sayLevel = [](int level) {
		const auto ms = Radar::WallRepeatMs[static_cast<size_t>(level)];
		if (!ms) return std::string(Strings::Current().radarWallOnce);
		char phrase[160]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().radarWallEvery, static_cast<int>(ms));
		return std::string(phrase);
	};
	return entry;
}

// How often the wall thud repeats while the stick pushes on (a tester, 2026-10-15).
SettingsList::Entry WallBumpLoop() {
	SettingsList::Entry entry;
	entry.key = "wall-bump-repeat";
	entry.levels = static_cast<int>(std::size(WallBump::LoopMs));
	entry.level = RadarService::BumpLoopLevel;
	entry.setLevel = RadarService::SetBumpLoopLevel;
	entry.sayLevel = [](int level) {
		const auto ms = WallBump::LoopMs[static_cast<size_t>(level)];
		if (!ms) return std::string(Strings::Current().wallBumpOnce);
		char phrase[160]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().wallBumpEvery, static_cast<int>(ms));
		return std::string(phrase);
	};
	return entry;
}

// The share of life under which the heartbeat sounds (a tester, 2026-10-15).
SettingsList::Entry LowLifeThreshold() {
	SettingsList::Entry entry;
	entry.key = "low-life-below";
	entry.levels = static_cast<int>(std::size(Vitals::LowLifeThresholds));
	entry.level = VitalsService::LowLifeLevel;
	entry.setLevel = VitalsService::SetLowLifeLevel;
	entry.sayLevel = [](int level) {
		char phrase[160]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().lowLifeBelow, Vitals::LowLifeThresholds[static_cast<size_t>(level)]);
		return std::string(phrase);
	};
	return entry;
}

SettingsList::Entry DropSoundSpeed() {
	SettingsList::Entry entry;
	entry.key = "drop-sound-speed";
	entry.levels = static_cast<int>(ItemAudio::DropSpeedPercents.size());
	entry.level = ItemAudio::DropSpeedLevel;
	entry.setLevel = ItemAudio::SetDropSpeedLevel;
	entry.sayLevel = [](int level) {
		char phrase[160]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().dropSoundSpeed, ItemAudio::DropSpeedPercents[static_cast<size_t>(level)]);
		return std::string(phrase);
	};
	return entry;
}

SettingsList::Entry FootstepVolume() {
	SettingsList::Entry entry;
	entry.key = "footstep-volume";
	entry.levels = static_cast<int>(Footsteps::VolumePercents.size());
	entry.level = FootstepService::VolumeLevel;
	entry.setLevel = FootstepService::SetVolumeLevel;
	entry.sayLevel = [](int level) {
		char phrase[160]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().footstepVolume, Footsteps::VolumePercents[static_cast<size_t>(level)]);
		return std::string(phrase);
	};
	return entry;
}

// How often the usable-corpse rattle repeats while corpses stay near (a tester, 2026-10-14).
SettingsList::Entry CorpseRepeat() {
	SettingsList::Entry entry;
	entry.key = "corpse-interval"; // milliseconds; "corpse-repeat" saved seconds
	entry.levels = static_cast<int>(EntityCues::CorpseRepeatMs.size());
	entry.level = EntityCueService::CorpseRepeatLevel;
	entry.setLevel = EntityCueService::SetCorpseRepeatLevel;
	entry.sayLevel = [](int level) {
		const int ms = EntityCues::CorpseRepeatMs[static_cast<size_t>(level)];
		if (!ms) return std::string(Strings::Current().corpseRepeatOnce);
		char phrase[160]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().corpseRepeatEvery, ms);
		return std::string(phrase);
	};
	return entry;
}

// The least equipment quality whose drop loop the loot filter keeps.
SettingsList::Entry LootEquipment() {
	static_assert(Strings::LootEquipmentCount == static_cast<size_t>(GroundItems::EquipmentLoops::Count), "a phrase for every equipment level");
	SettingsList::Entry entry;
	entry.key = "loot-equipment";
	entry.levels = static_cast<int>(GroundItems::EquipmentLoops::Count);
	entry.level = [] { return static_cast<int>(GroundItems::LootFilter::Equipment()); };
	entry.setLevel = [](int level) { GroundItems::LootFilter::SetEquipment(static_cast<GroundItems::EquipmentLoops>(level)); };
	entry.sayLevel = [](int level) { return std::string(Strings::Current().lootEquipment[static_cast<size_t>(level)]); };
	return entry;
}

// How far one kind of news may push past other speech: "Status effects ending: cuts in" (speech_priority.h).
SettingsList::Entry Priority(const char* key, SpeechPriority::Kind kind) {
	SettingsList::Entry entry;
	entry.key = key;
	entry.levels = SpeechPriority::LevelCount;
	entry.level = [kind] { return static_cast<int>(SpeechPriority::Get(kind)); };
	entry.setLevel = [kind](int level) { SpeechPriority::Set(kind, static_cast<SpeechPriority::Level>(level)); };
	entry.sayLevel = [kind](int level) {
		const auto& strings = Strings::Current();
		char text[300]{};
		std::snprintf(text, sizeof(text), strings.speechPriority, strings.speechPriorityKinds[static_cast<size_t>(kind)],
			strings.speechPriorityLevels[static_cast<size_t>(level)]);
		return std::string(text);
	};
	return entry;
}

// The groups of the settings list (a tester, 2026-10-13), in the order of
// Strings::Table::settingsGroups; the glossary is the last, the drop sounds a group inside it.
enum class SettingsGroup { Sounds, Enemies, Navigation, Items, Alerts, Controller, Glossary, Volumes, DropGlossary, SettingsMenu, LootFilter, LoopSpeeds, SpeechPriority, Count };
static_assert(static_cast<size_t>(SettingsGroup::Count) == Strings::SettingsGroupCount, "a name for every settings group");
static_assert(SpeechPriority::KindCount == Strings::SpeechPriorityKindCount && SpeechPriority::LevelCount == Strings::SpeechPriorityLevelCount,
	"a name for every kind of news and every priority");
static_assert(LoopPace::GroupCount == Strings::LoopPaceGroupCount, "a name for every loop pace group");

std::string GroupName(SettingsGroup group) { return Strings::Current().settingsGroups[static_cast<size_t>(group)]; }

// Every switch and setting kept between sessions, by group.
std::vector<SettingsList::Group> SettingGroups() {
	using SettingsList::Entry;
	using Cue = EntityCueService::Cue;
	using Item = GroundItemService::Switch;
	using T = Strings::Table;
	using G = SettingsGroup;
	const auto phrase = [](const char* T::* on, const char* T::* off) {
		return [on, off](bool state) { return std::string(Strings::Current().*(state ? on : off)); };
	};
	const auto cue = [](Cue which) { return [which] { return EntityCueService::Enabled(which); }; };
	const auto setCue = [](Cue which) { return [which](bool on) { EntityCueService::SetEnabled(which, on); }; };
	const auto item = [](Item which) { return [which] { return GroundItemService::Enabled(which); }; };
	const auto setItem = [](Item which) { return [which](bool on) { GroundItemService::SetEnabled(which, on); }; };
	auto strengths = Strengths(); // enemy pan, enemy pitch, beacon pan, beacon pitch, item pan
	std::vector<SettingsList::Group> groups;
	groups.push_back({GroupName(G::Sounds), {
		{ModSounds::Enabled, ModSounds::SetEnabled, phrase(&T::modSoundsOn, &T::modSoundsOff), false, "mod-sounds"},
		{item(Item::Sounds), setItem(Item::Sounds), [](bool on) { return std::string(GroundItemText::SoundEnabled(on)); }, true, "item-sounds"},
		ItemSoundSet(),
		std::move(strengths[4]),
		{GroundItems::PotionsLast, GroundItems::SetPotionsLast, phrase(&T::potionsLastOn, &T::potionsLastOff), false, "potions-last"},
		{GroundItemService::ShrineSounds, GroundItemService::SetShrineSounds, phrase(&T::shrineSoundsOn, &T::shrineSoundsOff), false, "shrine-sounds"},
		{item(Item::Quests), setItem(Item::Quests), [](bool on) { return std::string(GroundItemText::QuestSoundEnabled(on)); }, true, "quest-sounds"},
		{item(Item::Exits), setItem(Item::Exits), [](bool on) { return std::string(GroundItemText::ExitSoundEnabled(on)); }, true, "exit-sounds"},
		{item(Item::Stash), setItem(Item::Stash), [](bool on) { return std::string(GroundItemText::StashSoundEnabled(on)); }, true, "stash-sound"},
		{item(Item::Chests), setItem(Item::Chests), [](bool on) { return std::string(GroundItemText::ChestSoundEnabled(on)); }, true, "chest-sounds"},
		{item(Item::Doors), setItem(Item::Doors), [](bool on) { return std::string(GroundItemText::DoorSoundEnabled(on)); }, true, "door-sounds"},
		{GroundItemService::OpenDoorSounds, GroundItemService::SetOpenDoorSounds, phrase(&T::openDoorSoundsOn, &T::openDoorSoundsOff), false, "open-door-sounds"},
		{GroundItemService::DoorwaySounds, GroundItemService::SetDoorwaySounds, phrase(&T::doorwaySoundsOn, &T::doorwaySoundsOff), false, "doorway-sounds"},
		{GroundItemService::SecretDrafts, GroundItemService::SetSecretDrafts, phrase(&T::secretDraftsOn, &T::secretDraftsOff), false, "secret-drafts"},
		{GroundItemService::StepClicks, GroundItemService::SetStepClicks, phrase(&T::stepClicksOn, &T::stepClicksOff), false, "step-clicks"},
		StepClickInterval(),
		{GroundItemService::StepBeep, GroundItemService::SetStepBeep, phrase(&T::stepBeepOn, &T::stepBeepOff), false, "step-beep"},
		{TerrainService::RoomSounds, TerrainService::SetRoomSounds, phrase(&T::explorationSoundsOn, &T::explorationSoundsOff), false, "exploration-sounds"},
		{[] { return FootstepService::Enabled(); }, [](bool on) { FootstepService::SetEnabled(on); }, phrase(&T::footstepsEnabled, &T::footstepsDisabled), true, "footsteps"},
	}});
	using F = SoundVolume::Family;
	groups.push_back({GroupName(G::Volumes), {
		Volume("radar-volume", &T::radarVolume, F::Radar),
		Volume("enemy-volume", &T::enemyVolume, F::Enemies),
		Volume("ordinary-enemy-volume", &T::ordinaryEnemyVolume, F::OrdinaryEnemies),
		Volume("elite-enemy-volume", &T::eliteEnemyVolume, F::EliteEnemies),
		Volume("ranged-enemy-volume", &T::rangedEnemyVolume, F::RangedEnemies),
		Volume("shaman-enemy-volume", &T::shamanEnemyVolume, F::ShamanEnemies),
		Volume("boss-enemy-volume", &T::bossEnemyVolume, F::BossEnemies),
		Volume("item-volume", &T::itemVolume, F::Items),
		Volume("door-volume", &T::doorVolume, F::Doors),
		Volume("doorway-volume", &T::doorwayVolume, F::Doorways),
		Volume("guidance-volume", &T::guidanceVolume, F::Guidance),
		Volume("low-life-volume", &T::lowLifeVolume, F::LowLife),
		Volume("step-volume", &T::stepVolume, F::Steps),
		FootstepVolume(),
	}});
	// Every pulsing loop's pace in one place, next to the volumes (the user, 2026-10-16).
	using P = LoopPace::Group;
	groups.push_back({GroupName(G::LoopSpeeds), {
		LoopSpeed("door-loop-speed", P::Doors),
		LoopSpeed("small-item-loop-speed", P::SmallItems),
		LoopSpeed("container-loop-speed", P::Containers),
		LoopSpeed("shrine-loop-speed", P::Shrines),
		LoopSpeed("quest-loop-speed", P::Quest),
		LoopSpeed("room-loop-speed", P::Rooms),
		LoopSpeed("epoch-loop-speed", P::Epoch),
		DropSoundSpeed(),
		RadarWallRepeat(),
		WallBumpLoop(),
		CorpseRepeat(),
	}});
	groups.push_back({GroupName(G::Enemies), {
		{cue(Cue::All), setCue(Cue::All), phrase(&T::entityCuesEnabled, &T::entityCuesDisabled), true, "entity-cues"},
		{cue(Cue::Enemies), setCue(Cue::Enemies), phrase(&T::entityCueEnemiesEnabled, &T::entityCueEnemiesDisabled), true, "entity-cues-enemies"},
		{cue(Cue::Ordinary), setCue(Cue::Ordinary), phrase(&T::entityCueOrdinaryEnabled, &T::entityCueOrdinaryDisabled), true, "enemy-growls"},
		{EntityCueAudio::ClassLayers, EntityCueAudio::SetClassLayers, phrase(&T::enemyClassSoundsOn, &T::enemyClassSoundsOff), false, "enemy-class-sounds"},
		std::move(strengths[0]),
		std::move(strengths[1]),
		{cue(Cue::Ranged), setCue(Cue::Ranged), phrase(&T::entityCueRangedEnabled, &T::entityCueRangedDisabled), true, "entity-cues-ranged"},
		{cue(Cue::Reach), setCue(Cue::Reach), phrase(&T::entityCueReachEnabled, &T::entityCueReachDisabled), true, "attack-reach"},
		{cue(Cue::Crosshair), setCue(Cue::Crosshair), phrase(&T::crosshairOn, &T::crosshairOff), true, "crosshair"},
		{BossLifeService::Enabled, BossLifeService::SetEnabled, phrase(&T::bossLifeEnabled, &T::bossLifeDisabled), true, "boss-life"},
		{cue(Cue::Npcs), setCue(Cue::Npcs), phrase(&T::entityCueNpcsEnabled, &T::entityCueNpcsDisabled), true, "entity-cues-npcs"},
		{EntityRadarService::NpcEnabled, EntityRadarService::SetNpcEnabled, phrase(&T::npcEnabled, &T::npcDisabled), true, "npc-announcer"},
		{EntityCueService::NpcAlerts, EntityCueService::SetNpcAlerts, phrase(&T::npcAlertOn, &T::npcAlertOff), false, "npc-alerts"},
		{EntityCueService::CorpseCue, EntityCueService::SetCorpseCue, phrase(&T::corpseCueOn, &T::corpseCueOff), false, "corpse-cue"},
	}});
	groups.push_back({GroupName(G::Navigation), {
		{[] { return RadarService::Enabled(); }, [](bool on) { RadarService::SetEnabled(on); }, phrase(&T::radarEnabled, &T::radarDisabled), true, "radar"},
		DirectionStyle(),
		{GroundItemService::DistanceSteps, GroundItemService::SetDistanceSteps, phrase(&T::distanceStepsOn, &T::distanceStepsOff), false, "distance-steps"},
		{RadarService::Diagonal, RadarService::SetDiagonal, [](bool diagonal) {
			return RadarReadout::RaysPhrase(diagonal ? Radar::Frame::Diagonal : Radar::Frame::Cardinal, Strings::Current()); }, true, "radar-diagonal"},
		{RadarService::BumpEnabled, RadarService::SetBumpEnabled, phrase(&T::wallBumpEnabled, &T::wallBumpDisabled), false, "wall-bump"},
		{RadarService::SmallObstaclesEnabled, RadarService::SetSmallObstaclesEnabled, phrase(&T::smallObstaclesEnabled, &T::smallObstaclesDisabled), false, "small-obstacles"},
		{WallTones::Enabled, WallTones::SetEnabled, phrase(&T::wallTonesOn, &T::wallTonesOff), false, "wall-tones"},
		{[] { return WallSlideService::Enabled(); }, [](bool on) { WallSlideService::SetEnabled(on); }, phrase(&T::movementEnabled, &T::movementDisabled), true, "wall-slide"},
		{WallSlideService::NarrowSlide, WallSlideService::SetNarrowSlide, phrase(&T::narrowSlideOn, &T::narrowSlideOff), false, "narrow-slide"},
		{BeaconService::Sounds, BeaconService::SetSounds, phrase(&T::beaconSoundsOn, &T::beaconSoundsOff), true, "beacon-sounds"},
		{BeaconService::RoutePrompts, BeaconService::SetRoutePrompts, phrase(&T::routePromptsOn, &T::routePromptsOff), false, "route-prompts"},
		{BeaconService::RouteTotal, BeaconService::SetRouteTotal, phrase(&T::routeTotalOn, &T::routeTotalOff), false, "route-total"},
		{BeaconService::RouteLegsSaid, BeaconService::SetRouteLegsSaid, phrase(&T::routeLegsOn, &T::routeLegsOff), false, "route-legs"},
		{BeaconService::RouteLongLegs, BeaconService::SetRouteLongLegs, [](bool on) {
			char said[160]{};
			std::snprintf(said, sizeof(said), on ? Strings::Current().routeLongLegsOn : Strings::Current().routeLongLegsOff,
				static_cast<int>(RouteLegs::MinLegUnits));
			return std::string(said); }, false, "route-long-legs"},
		std::move(strengths[2]),
		std::move(strengths[3]),
		{NavigationMode::FullAssistance, BeaconService::SetFullAssistance, phrase(&T::navigationFullAssistance, &T::navigationExploration), true, "full-assistance"},
		{[] { return RoomService::Enabled(); }, [](bool on) { RoomService::SetEnabled(on); }, phrase(&T::roomsEnabled, &T::roomsDisabled), true, "rooms"},
		{RoomService::MapSides, RoomService::SetMapSides, phrase(&T::mapSidesOn, &T::mapSidesOff), false, "passage-map-sides"},
		{PlaceService::Enabled, PlaceService::SetEnabled, phrase(&T::placesEnabled, &T::placesDisabled), true, "places"},
		{Descriptions::Enabled, Descriptions::SetEnabled, phrase(&T::descriptionsOn, &T::descriptionsOff), false, "descriptions"},
		{Descriptions::HintEnabled, Descriptions::SetHintEnabled, phrase(&T::descriptionHintOn, &T::descriptionHintOff), false, "description-hint"},
		{TerrainService::PlaceAlerts, TerrainService::SetPlaceAlerts, phrase(&T::discoveriesOn, &T::discoveriesOff), false, "discoveries"},
		{MapSurveyService::ReachFilter, MapSurveyService::SetReachFilter, phrase(&T::reachFilterOn, &T::reachFilterOff), false, "reach-filter"},
		{MapSurveyService::TownNpcsEverywhere, MapSurveyService::SetTownNpcsEverywhere, phrase(&T::townNpcsEverywhereOn, &T::townNpcsEverywhereOff), false, "town-npcs-everywhere"},
		{MapSurveyService::UnexploredSpread, MapSurveyService::SetUnexploredSpread, phrase(&T::unexploredSpreadOn, &T::unexploredSpreadOff), false, "unexplored-spread"},
		{MapSurveyService::UnexploredEverywhere, MapSurveyService::SetUnexploredEverywhere, phrase(&T::unexploredEverywhereOn, &T::unexploredEverywhereOff), false, "unexplored-everywhere"},
		{MapSurveyService::DoorwaysListed, MapSurveyService::SetDoorwaysListed, phrase(&T::doorwaysListedOn, &T::doorwaysListedOff), false, "doorways-listed"},
	}});
	groups.push_back({GroupName(G::Items), {
		{item(Item::Speech), setItem(Item::Speech), [](bool on) { return std::string(GroundItemText::Enabled(on)); }, true, "item-speech"},
		{GroundItemText::SilverEnabled, GroundItemText::SetSilverEnabled, phrase(&T::silverItemsOn, &T::silverItemsOff), true, "silver-items"},
		{AutoDetail::Enabled, AutoDetail::SetEnabled, phrase(&T::autoDetailEnabled, &T::autoDetailDisabled), true, "auto-detail"},
		{AutoDetail::ControlTooltips, AutoDetail::SetControlTooltips, phrase(&T::controlTooltipsEnabled, &T::controlTooltipsDisabled), true, "control-tooltips"},
		{InteractionReader::HandSpeech, InteractionReader::SetHandSpeech, phrase(&T::handSpeechOn, &T::handSpeechOff), false, "hand-speech"},
		{InteractionReader::CompareAloud, InteractionReader::SetCompareAloud, phrase(&T::compareAloudOn, &T::compareAloudOff), false, "compare-aloud"},
		{InteractionReader::CompareHint, InteractionReader::SetCompareHint, phrase(&T::compareHintOn, &T::compareHintOff), false, "compare-hint"},
		{GridEdgeService::Enabled, GridEdgeService::SetEnabled, phrase(&T::gridEdgeOn, &T::gridEdgeOff), false, "inventory-edge"},
		{InventoryArrivalService::StashNotice, InventoryArrivalService::SetStashNotice, phrase(&T::stashNoticeOn, &T::stashNoticeOff), false, "stash-notice"},
		{CharacterStatsReader::FocusPoints, CharacterStatsReader::SetFocusPoints, phrase(&T::statPointsFocusOn, &T::statPointsFocusOff), false, "stat-points-focus"},
		{CharacterStatsReader::ZeroPoints, CharacterStatsReader::SetZeroPoints, phrase(&T::zeroPointsOn, &T::zeroPointsOff), false, "zero-points"},
		{SkillNavigation::Enabled, SkillNavigation::SetEnabled, phrase(&T::skillGridOn, &T::skillGridOff), false, "skill-grid"},
		{[] { return SkillBarService::Enabled(); }, [](bool on) { SkillBarService::SetEnabled(on); }, phrase(&T::skillBarReaderOn, &T::skillBarReaderOff), false, "skill-bar"},
		{FocusTracker::ScreenHints, FocusTracker::SetScreenHints, phrase(&T::screenHintsOn, &T::screenHintsOff), false, "screen-hints"},
		{GameTextReader::Enabled, GameTextReader::SetEnabled, phrase(&T::gameTextEnabled, &T::gameTextDisabled), true, "game-text"},
	}});
	groups.push_back({GroupName(G::Alerts), {
		{[] { return VitalsService::Enabled(); }, [](bool on) { VitalsService::SetEnabled(on); }, phrase(&T::vitalsEnabled, &T::vitalsDisabled), true, "vitals"},
		ManaAlerts(),
		{VitalsService::ExperienceReport, VitalsService::SetExperienceReport, phrase(&T::experienceReportOn, &T::experienceReportOff), false, "experience-report"},
		{VitalsService::StatusAlerts, VitalsService::SetStatusAlerts, phrase(&T::statusAlertsOn, &T::statusAlertsOff), false, "status-alerts"},
		{VitalsService::LowLifeSound, VitalsService::SetLowLifeSound, phrase(&T::lowLifeOn, &T::lowLifeOff), false, "low-life-sound"},
		LowLifeThreshold(),
		{VitalsService::MinionsReported, VitalsService::SetMinionsReported, phrase(&T::minionReportOn, &T::minionReportOff), false, "minion-report"},
		{VitalsService::MercenaryReported, VitalsService::SetMercenaryReported, phrase(&T::mercenaryReportOn, &T::mercenaryReportOff), false, "mercenary-report"},
		{VitalsService::PartyFirst, VitalsService::SetPartyFirst, phrase(&T::partyFirstOn, &T::partyFirstOff), false, "party-first"},
		{VitalsService::SkillsReported, VitalsService::SetSkillsReported, phrase(&T::skillReportOn, &T::skillReportOff), false, "skill-report"},
		{VitalsService::MovementAlerts, VitalsService::SetMovementAlerts, phrase(&T::movementAlertsOn, &T::movementAlertsOff), false, "movement-alerts"},
		{VitalsService::AmmoAlerts, VitalsService::SetAmmoAlerts, phrase(&T::ammoAlertsOn, &T::ammoAlertsOff), false, "ammo-alerts"},
		{[] { return QuestService::Enabled(); }, [](bool on) { QuestService::SetEnabled(on); }, phrase(&T::questAlertsEnabled, &T::questAlertsDisabled), true, "quest-alerts"},
		{GameTextReader::MercenaryBark, GameTextReader::SetMercenaryBark, phrase(&T::mercenaryBarkOn, &T::mercenaryBarkOff), false, "mercenary-bark"},
		{[] { return TradeService::Enabled(); }, [](bool on) { TradeService::SetEnabled(on); }, phrase(&T::tradeAlertsOn, &T::tradeAlertsOff), false, "trade-alerts"},
		{[] { return WeaponSwapService::Enabled(); }, [](bool on) { WeaponSwapService::SetEnabled(on); }, phrase(&T::weaponSwapAlertsOn, &T::weaponSwapAlertsOff), false, "weapon-swap-alerts"},
		{SkillHotkeyService::Enabled, SkillHotkeyService::SetEnabled, phrase(&T::skillHotkeysOn, &T::skillHotkeysOff), false, "skill-hotkeys"},
		{AudioDescription::Enabled, AudioDescription::SetEnabled, phrase(&T::audioDescriptionOn, &T::audioDescriptionOff), false, "audio-description"},
		{TutorialService::Enabled, TutorialService::SetEnabled, phrase(&T::tutorialOn, &T::tutorialOff), false, "tutorial"},
		{TutorialService::WaitForSounds, TutorialService::SetWaitForSounds, phrase(&T::tutorialSoundWaitOn, &T::tutorialSoundWaitOff), false, "tutorial-sound-wait"},
		{WeaponSwapService::SetNumber, WeaponSwapService::SetSetNumber, phrase(&T::weaponSetNumberOn, &T::weaponSetNumberOff), false, "weapon-set-number"},
		{ModalReader::MenuNews, ModalReader::SetMenuNews, phrase(&T::menuNewsOn, &T::menuNewsOff), false, "main-menu-news"},
		{[] { return g_updateCheck.load(); }, [](bool on) { g_updateCheck = on; }, phrase(&T::updateCheckOn, &T::updateCheckOff), false, "update-check"},
	}});
	// Which news may cut in on other speech, kind by kind (a tester and the user, 2026-10-19).
	using K = SpeechPriority::Kind;
	groups.push_back({GroupName(G::SpeechPriority), {
		Priority("priority-status-begins", K::StatusBegins),
		Priority("priority-status-ends", K::StatusEnds),
		Priority("priority-life-alerts", K::LifeAlerts),
		Priority("priority-boss-life", K::BossLife),
		Priority("priority-enemy-names", K::EnemyNames),
		Priority("priority-game-messages", K::GameNotices),
		Priority("priority-quest-updates", K::QuestAlerts),
		Priority("priority-places", K::Places),
		Priority("priority-ground-items", K::NearbyItems),
		Priority("priority-tutorial", K::Tutorial),
		Priority("priority-arrivals", K::Arrivals),
	}});
	groups.push_back({GroupName(G::Controller), {
		{PadFilter::KeepBack, PadFilter::SetKeepBack, phrase(&T::backToModOn, &T::backToModOff), false, "pad-back-to-mod"},
		{InputDirection::RouteOnA, InputDirection::SetRouteOnA, phrase(&T::routeOnAOn, &T::routeOnAOff), false, "pad-route-on-a"},
		{PadTyping::Enabled, PadTyping::SetEnabled, phrase(&T::padTypingOn, &T::padTypingOff), false, "pad-typing"},
		{PadTyping::Qwerty, PadTyping::SetQwerty, phrase(&T::padQwertyOn, &T::padQwertyOff), false, "pad-typing-qwerty"},
	}});
	// The list's own behaviour, in a group of its own (a tester, 2026-10-14).
	groups.push_back({GroupName(G::SettingsMenu), {
		{[] { return g_settingsStop.load(); }, [](bool on) { g_settingsStop = on; }, phrase(&T::settingsEndsStop, &T::settingsEndsWrap), false, "settings-list-stops"},
		{[] { return g_settingsRestart.load(); }, [](bool on) { g_settingsRestart = on; }, phrase(&T::settingsRestartOn, &T::settingsRestartOff), false, "settings-list-restart"},
	}});
	// Which drop loops the audible loot filter keeps while it is on (LB in play; a tester, 2026-10-14).
	namespace Loot = GroundItems::LootFilter;
	groups.push_back({GroupName(G::LootFilter), {
		{Loot::Active, Loot::SetActive, phrase(&T::lootFilterOn, &T::lootFilterOff), false, "loot-filter"},
		{Loot::Potions, Loot::SetPotions, phrase(&T::lootPotionsOn, &T::lootPotionsOff), false, "loot-potions"},
		{Loot::Gems, Loot::SetGems, phrase(&T::lootGemsOn, &T::lootGemsOff), false, "loot-gems"},
		LootEquipment(),
	}});
	return groups;
}

// The same switches as one list, for keeping them between sessions.
std::vector<SettingsList::Entry> SettingEntries() {
	std::vector<SettingsList::Entry> entries;
	for (auto& group : SettingGroups()) for (auto& entry : group.entries) entries.push_back(std::move(entry));
	return entries;
}

// The switches, then the sound glossary: sounds to hear, nothing to save.
SettingsList::Menu BuildSettings() {
	auto groups = SettingGroups();
	groups.push_back({GroupName(SettingsGroup::Glossary), SoundGlossary::Entries(),
		{{GroupName(SettingsGroup::DropGlossary), SoundGlossary::DropEntries(), {}}}});
	SettingsList::Menu menu(std::move(groups), Strings::Current().settingsOpened, Strings::Current().settingsClosed);
	menu.SetGroupFormat(Strings::Current().settingsSubmenu);
	return menu;
}

// ---- the switches kept between sessions ----

// The saved switches are put back at start one at a time, each once the one
// before has taken: two of the radar's share one command. What each says as
// it is put back is not spoken. After that a change of any switch, from the
// list or from its own key, is written within a second; a switch never
// changed keeps what the file has, even one that did not take this session.
constexpr uint64_t SettingTakeMs = 2000;   // how long a switch put back may take before the next
constexpr uint64_t SettingQuietMs = 5000;  // how long its own announcement is listened for
constexpr uint64_t SettingCheckMs = 1000;  // how often the switches are looked at for a change
std::filesystem::path g_settingsFile;
std::vector<SettingsList::Entry> g_switches;
std::map<std::string, int> g_saved;
std::vector<std::pair<size_t, int>> g_restore;
size_t g_restoreAt = 0;
uint64_t g_restoreSince = 0, g_settingsCheck = 0;
std::vector<int> g_seen;
bool g_settingsLoaded = false;

// The percent settings saved before they all moved in quarters (2026-10-14), by
// their levels then, so a file saved before keeps each value it had.
constexpr std::array OldVolumePercents{0, 25, 50, 75, 100, 125, 150, 200};
constexpr std::array OldDropSpeedPercents{50, 75, 100, 150, 200, 300};
constexpr std::array OldFootstepPercents{25, 50, 75, 100, 150, 200, 300};
constexpr const char* QuarterStepsKey = "quarter-steps";

template<size_t Old, size_t New>
void ToQuarters(const char* key, const std::array<int, Old>& old, const std::array<int, New>& now) {
	const auto found = g_saved.find(key);
	if (found == g_saved.end() || found->second < 0 || static_cast<size_t>(found->second) >= Old) return;
	const int level = PercentSteps::LevelOf(now, old[static_cast<size_t>(found->second)]);
	if (level >= 0) found->second = level;
}

void SavedToQuarters() {
	if (g_saved.contains(QuarterStepsKey)) return;
	for (const char* key : {"radar-volume", "enemy-volume", "item-volume", "guidance-volume"})
		ToQuarters(key, OldVolumePercents, SoundVolume::Percents);
	ToQuarters("drop-sound-speed", OldDropSpeedPercents, ItemAudio::DropSpeedPercents);
	ToQuarters("footstep-volume", OldFootstepPercents, Footsteps::VolumePercents);
	g_saved[QuarterStepsKey] = 1;
}

void Persist(uint64_t now) {
	if (g_settingsFile.empty()) return;
	if (!g_settingsLoaded) {
		g_settingsLoaded = true;
		g_switches = SettingEntries();
		if (const auto text = SettingsStore::Load(g_settingsFile)) g_saved = SettingsStore::Decode(*text);
		// The two item sound switches became one set (2026-10-13): a file saved before keeps its choice.
		if (!g_saved.contains("item-sound-set")) {
			if (g_saved["last-epoch-sounds"] == 1) g_saved["item-sound-set"] = static_cast<int>(GroundItems::SoundSet::LastEpoch);
			g_saved.erase("last-epoch-sounds");
		}
		SavedToQuarters();
		for (size_t i = 0; i < g_switches.size(); ++i)
			if (const auto found = g_switches[i].key ? g_saved.find(g_switches[i].key) : g_saved.end();
				found != g_saved.end() && g_switches[i].Value() != found->second) g_restore.push_back({i, found->second});
		AsyncLog::InfoF(g_context, "[settings] %zu saved, %zu to put back, file %s", g_saved.size(), g_restore.size(),
			g_settingsFile.string().c_str());
	}
	if (g_restoreAt < g_restore.size()) {
		const auto [index, value] = g_restore[g_restoreAt];
		const auto& entry = g_switches[index];
		if (!g_restoreSince) {
			ScreenReader::Swallow(entry.Say(value), now + SettingQuietMs);
			entry.Put(value);
			g_restoreSince = now;
			AsyncLog::InfoF(g_context, "[settings] put back %s=%d", entry.key, value);
			return;
		}
		if (entry.Value() != value && now - g_restoreSince < SettingTakeMs) return;
		if (entry.Value() != value) AsyncLog::InfoF(g_context, "[settings] %s did not take", entry.key);
		++g_restoreAt;
		g_restoreSince = 0;
		return;
	}
	if (now < g_settingsCheck) return;
	g_settingsCheck = now + SettingCheckMs;
	bool changed = false;
	if (g_seen.empty()) {
		for (const auto& entry : g_switches) g_seen.push_back(entry.Value());
		return;
	}
	for (size_t i = 0; i < g_switches.size(); ++i) {
		const int value = g_switches[i].Value();
		if (value == g_seen[i]) continue;
		g_seen[i] = value;
		if (g_switches[i].key) { g_saved[g_switches[i].key] = value; changed = true; }
	}
	if (!changed) return;
	const bool saved = SettingsStore::Save(g_settingsFile, SettingsStore::Encode(g_saved));
	AsyncLog::InfoF(g_context, "[settings] %s %zu switches", saved ? "saved" : "could not save", g_saved.size());
}

// True when the command was the list's and is spent.
bool Settings(Command command, uint64_t now) {
	if (!g_settingsBuilt || g_settingsLanguage != Strings::Selected()) {
		const bool open = g_settings.Open();
		g_settings = BuildSettings();
		g_settingsLanguage = Strings::Selected();
		g_settingsBuilt = true;
		if (open) (void)g_settings.Toggle(now);
	}
	std::string text;
	bool spent = true;
	g_settings.SetRestart(g_settingsRestart.load());
	if (command == Command::Settings) text = g_settings.Toggle(now);
	else if (!g_settings.Open()) spent = false;
	else if (command == Command::SettingsChange) text = g_settings.Enter(now);
	else if (command == Command::SettingsBack) text = g_settings.Back(now);
	else if (command == Command::SettingsLower || command == Command::SettingsRaise)
		text = g_settings.Change(now, command == Command::SettingsLower ? -1 : 1);
	else if (command == Command::ReadPrevious || command == Command::ReadFirst || command == Command::ReadNext
		|| command == Command::ReadLast) {
		const bool back = command == Command::ReadPrevious || command == Command::ReadFirst;
		text = g_settings.Step(back ? -1 : 1, now, g_settingsStop.load() ? SettingsList::Ends::Stop : SettingsList::Ends::Wrap);
		if (g_settings.LastEdge() != SettingsList::Edge::None)
			EntityCueAudio::Play(g_settings.LastEdge() == SettingsList::Edge::Wrapped ? EntityCueAudio::Sound::Pickup
				: EntityCueAudio::Sound::Edge, {});
	} else spent = false;
	if (!spent) text = g_settings.Expire(now, SettingsIdleMs);
	g_settingsOpen = g_settings.Open();
	PadFilter::SetSettingsOpen(g_settingsOpen);
	Persist(now);
	if (!text.empty()) {
		AsyncLog::InfoF(g_context, "[keyboard] settings: %s", text.c_str());
		ScreenReader::Speak(text, true);
	}
	return spent;
}

// The pad's Ctrl+S, Page Up/Down, Space, Backspace and Shift+Space: BACK+RB opens
// and shuts the list, in play (where the pad filter keeps that RB from the game)
// or to shut it anywhere, and while it is open the right stick steps through it
// (up, down), opens a group or changes a switch (right) and goes back to the
// groups (left) before the scanner or a menu can take the push; LB and RB, kept
// from the game while it is open, lower and raise a setting. Every press is taken
// every update, so an old one is never acted on.
Command PadSettings(bool inPlay) {
	using InputDirection::StickPush;
	if (InputDirection::TakeBackWithRb() && (inPlay || g_settingsOpen.load())) return Command::Settings;
	const bool lower = InputDirection::TakeShoulder(false), raise = InputDirection::TakeShoulder(true);
	// With the list shut, LB in play is the loot filter's, as sighted players tap it to filter the floor.
	if (!g_settingsOpen.load()) return lower && inPlay ? Command::LootFilter : Command::None;
	if (lower != raise) return lower ? Command::SettingsLower : Command::SettingsRaise;
	if (InputDirection::PeekRightStickPush() == StickPush::None) return Command::None;
	switch (InputDirection::TakeRightStickPush("settings")) {
	case StickPush::Up:   return Command::ReadPrevious;
	case StickPush::Down: return Command::ReadNext;
	case StickPush::Left:  return Command::SettingsBack;
	case StickPush::Right: return Command::SettingsChange;
	default: return Command::None;
	}
}

// ---- console: the first live tests ----

void Test(const Lifecycle& state, Command command) {
	if (command == Command::LogOn || command == Command::LogOff) {
		NativeActions::SetLogging(command == Command::LogOn);
		return;
	}
	if (command == Command::TestStatus) {
		AsyncLog::InfoF(g_context, "[keyboard] status: actions=%u left_skill=%d right_skill=%d mode=%d level=%d came_from=%d",
			NativeActions::Available() ? 1u : 0u, NativeActions::SelectedSkill(Hand::Left),
			NativeActions::SelectedSkill(Hand::Right), static_cast<int>(g_mode), state.level, state.cameFrom);
		return;
	}
	if (command == Command::TestHalt) { (void)NativeActions::Halt(); return; }
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) {
		AsyncLog::Info(g_context, "[keyboard] test: the world could not be read");
		return;
	}
	const Position aim{world.player.x + g_testX.load(), world.player.y + g_testY.load()};
	AsyncLog::InfoF(g_context, "[keyboard] test: from %.0f,%.0f to %.0f,%.0f %s", world.player.x, world.player.y, aim.x, aim.y,
		command == Command::TestPlain ? "plain" : "as a click");
	(void)NativeActions::MoveTo(aim, true, command == Command::TestPlain);
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	std::istringstream words(args);
	std::string word;
	int x = 0, y = 0, read = 0;
	if (words >> word) ++read;
	if (read && (words >> x)) ++read;
	if (read == 2 && (words >> y)) ++read;
	if (read >= 1 && (word == "run" || word == "plain")) {
		if (read != 3) return D2RL::ConsoleCommandResult::InvalidArguments;
		g_testX = x;
		g_testY = y;
		g_deferred = word == "run" ? Command::TestRun : Command::TestPlain;
	} else if (word == "halt") g_deferred = Command::TestHalt;
	else if (word == "status") g_command = Command::TestStatus;
	else if (args == "log on") g_command = Command::LogOn;
	else if (args == "log off") g_command = Command::LogOff;
	else if (word == "explore") g_deferred = Command::Explore;
	else if (word == "retreat") g_deferred = Command::Retreat;
	else if (word == "stop") g_deferred = Command::Stop;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}

auto __cdecl OnAction(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* userData) noexcept
	-> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize)) return D2RL::Input::ActionResult::Ignored;
	auto command = static_cast<Command>(reinterpret_cast<uintptr_t>(userData));
	const bool pressed = event->kind == D2RL::Input::ActionEventKind::Pressed;
	// Space changes a switch of the settings list and is the game's otherwise.
	if ((command == Command::SettingsChange || command == Command::SettingsBack || command == Command::SettingsLower)
		&& !g_settingsOpen.load()) return D2RL::Input::ActionResult::Ignored;
	// With the keyboard's cursor on a box of text the keys that type are the
	// box's: a letter alone or with Shift, and the keys that move in the text.
	if (KeyboardMenu::Typing() && !g_settingsOpen.load()
		&& (event->binding.modifier == D2RL::Input::Modifier::None || event->binding.modifier == D2RL::Input::Modifier::Shift)) {
		if (!pressed && (command == Command::Attack || command == Command::Cast)) g_held[command == Command::Attack ? 0 : 1] = 0;
		return D2RL::Input::ActionResult::Ignored;
	}
	// In a menu the keys are the menu's: a click, a tab, a button (keyboard_menu.h).
	if (!g_playing.load() && !g_settingsOpen.load() && KeyboardMenu::Active()) {
		using KeyboardMenu::Act;
		if (command == Command::Attack || command == Command::Cast) {
			const size_t hand = command == Command::Attack ? 0 : 1;
			// A held key repeats its press; only the first one is a click. W is
			// kept from the game and does nothing here: the game's W swaps weapons.
			if (pressed && g_held[hand].exchange(static_cast<uint32_t>(event->binding.key)) == 0 && hand == 0)
				KeyboardMenu::Deliver(Act::ClickLeft);
			if (!pressed) g_held[hand] = 0;
			return pressed ? D2RL::Input::ActionResult::Handled : D2RL::Input::ActionResult::Ignored;
		}
		// D is the right click in a menu, as it is the hand that uses a thing in play.
		if (command == Command::Interact) command = Command::ClickRight;
		const char* button = command == Command::Retreat ? RepairAllButton : command == Command::Repair ? RepairButton
			: command == Command::Transmute ? TransmuteButton : nullptr;
		const bool menuKey = button || command == Command::ClickLeft || command == Command::ClickRight
			|| command == Command::ReadPrevious || command == Command::ReadNext || command == Command::Repeat || command == Command::Stop
			|| command == Command::ReadDetail || command == Command::StashPagePrevious || command == Command::StashPageNext;
		if (menuKey || command == Command::Sort || command == Command::FillBelt) {
			if (!pressed) return D2RL::Input::ActionResult::Ignored;
			if (command == Command::Sort) g_sort = true;
			else if (command == Command::FillBelt) g_fill = true;
			else if (button) g_press = button;
			else KeyboardMenu::Deliver(command == Command::ClickLeft ? Act::ClickLeft : command == Command::ClickRight ? Act::ClickRight
				: command == Command::ReadPrevious ? Act::TabPrevious : command == Command::ReadNext ? Act::TabNext
				: command == Command::ReadDetail ? Act::Detail
				: command == Command::StashPagePrevious ? Act::PagePrevious : command == Command::StashPageNext ? Act::PageNext
				: command == Command::Repeat ? Act::MenuPrevious : Act::MenuNext);
			return D2RL::Input::ActionResult::Handled;
		}
	}
	// Alt+Page keys are the stash's in a menu (above), the game's elsewhere.
	if (command == Command::StashPagePrevious || command == Command::StashPageNext) return D2RL::Input::ActionResult::Ignored;
	// Ctrl+Shift+Page keys are the menu's page jump, seen directly (keyboard_menu.cpp); the loader's one-modifier reads stay silent.
	if ((command == Command::ReadFirst || command == Command::ReadLast || command == Command::CompareItem || command == Command::CompareEquipped)
		&& KeyboardMenu::Active() && !g_playing.load() && (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000))
		return D2RL::Input::ActionResult::Handled;
	// Shift+A is the equip click in a menu (above); in play it attacks in place.
	if (command == Command::ClickLeft && event->binding.modifier == D2RL::Input::Modifier::Shift) command = Command::Attack;
	// Other modified clicks and the cube's key are the game's keys out of a menu.
	if (command == Command::ClickLeft || command == Command::ClickRight || command == Command::Transmute || command == Command::Sort
		|| command == Command::FillBelt || command == Command::Repair)
		return D2RL::Input::ActionResult::Ignored;
	// A letter typed into a line of chat is the chat's.
	if ((command == Command::Swap || command == Command::LeftSkill || command == Command::ItemNext || command == Command::ItemPrevious
		|| command == Command::NpcNext || command == Command::NpcPrevious) && NativeSkillNavigation::TextEditing().value_or(false))
		return D2RL::Input::ActionResult::Ignored;
	const bool reading = command == Command::Swap || command == Command::LeftSkill
		|| command == Command::ReadPrevious || command == Command::ReadNext || command == Command::ReadFirst
		|| command == Command::ReadLast || command == Command::CompareItem || command == Command::CompareEquipped
		|| command == Command::Repeat || command == Command::Settings || command == Command::SettingsChange || command == Command::SettingsBack
		|| command == Command::SettingsLower;
	if (!reading && !g_playing.load()) {
		// A release still has to end a hold begun in play.
		if (!pressed && (command == Command::Attack || command == Command::Cast)) g_held[command == Command::Attack ? 0 : 1] = 0;
		return D2RL::Input::ActionResult::Ignored;
	}
	if (command == Command::Attack || command == Command::Cast) {
		const size_t hand = command == Command::Attack ? 0 : 1;
		// A held key repeats its press; only the first one begins a fight.
		if (pressed && g_held[hand].exchange(static_cast<uint32_t>(event->binding.key)) == 0) g_pressed[hand] = true;
		if (!pressed) g_held[hand] = 0;
		return pressed ? D2RL::Input::ActionResult::Handled : D2RL::Input::ActionResult::Ignored;
	}
	if (!pressed) return D2RL::Input::ActionResult::Ignored;
	g_command = command;
	return D2RL::Input::ActionResult::Handled;
}

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = g_state.cameFrom = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = g_state.cameFrom = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) {
		// The level left is the way back; the first level of a game has none.
		g_state.cameFrom = event->previousValue > 0 ? event->previousValue : 0;
		g_state.level = event->currentValue;
	}
	++g_state.revision;
}

// A key the game did not report released: the window lost focus, or a menu
// took the keyboard. The key itself says whether it is still down.
void ReleaseLostHolds() {
	for (auto& held : g_held) {
		const uint32_t key = held.load();
		if (key && !Down(static_cast<int>(key))) held = 0;
	}
}
}

bool Register(const D2RL::PluginContext* context, bool nativeHooks) {
	if (!context) return false;
	g_context = context;
	if (context->pluginDirectory) g_settingsFile = std::filesystem::path(context->pluginDirectory) / "d2r-access" / "settings.txt";
	(void)NativeActions::Initialize(context, nativeHooks);
	if (nativeHooks) (void)NativeSort::Initialize(context);
	if (nativeHooks) (void)NativeSwap::Initialize(context);
	if (nativeHooks) (void)NativeBeltRows::Initialize(context);
	if (nativeHooks) (void)NativeBeltFill::Initialize(context);
	if (nativeHooks) (void)NativeSkillHand::Initialize(context);
	if (nativeHooks) (void)NativeSkillIcons::Initialize(context);
	if (nativeHooks) (void)NativeValueControls::Initialize(context);
	if (nativeHooks) (void)NativeClassChoice::Initialize(context);
	if (nativeHooks) (void)NativeListRows::Initialize(context);
	if (nativeHooks) (void)NativeStashPages::Initialize(context);
	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) { Unregister(); return false; }
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		using D2RL::Input::Key;
		using D2RL::Input::Modifier;
		struct Entry { const char* id; const char* label; Key key; Modifier modifier; Command command; };
		size_t registered = 0, offered = 0;
		for (const Entry& entry : {
			Entry{"keyboard-attack", "Fight with the left skill (hold)", Key::A, Modifier::None, Command::Attack},
			Entry{"keyboard-cast", "Fight with the right skill (hold)", Key::W, Modifier::None, Command::Cast},
			Entry{"keyboard-target-next", "Next monster", Key::Tab, Modifier::None, Command::TargetNext},
			Entry{"keyboard-target-previous", "Previous monster", Key::Tab, Modifier::Shift, Command::TargetPrevious},
			Entry{"keyboard-item-next", "Next item or object on screen, for D to use", Key::Period, Modifier::None, Command::ItemNext},
			Entry{"keyboard-item-previous", "Previous item or object on screen, for D to use", Key::Comma, Modifier::None, Command::ItemPrevious},
			Entry{"keyboard-npc-next", "Next NPC on screen, for D to talk to", Key::RightBracket, Modifier::None, Command::NpcNext},
			Entry{"keyboard-npc-previous", "Previous NPC on screen, for D to talk to", Key::LeftBracket, Modifier::None, Command::NpcPrevious},
			Entry{"keyboard-use", "Walk up to the nearest thing and use it; in a menu, right click", Key::D, Modifier::None, Command::Interact},
			Entry{"keyboard-walk", "Walk to the scanned target", Key::Home, Modifier::Shift, Command::WalkToTarget},
			Entry{"keyboard-explore", "Walk to unexplored ground", Key::H, Modifier::None, Command::Explore},
			Entry{"keyboard-retreat", "Retreat to the way you came in; in a menu, repair all", Key::R, Modifier::Control, Command::Retreat},
			Entry{"keyboard-stop", "Stop walking or fighting", Key::End, Modifier::None, Command::Stop},
			Entry{"keyboard-room", "Say the ways out of this room", Key::Y, Modifier::Control, Command::RoomLayout},
			Entry{"keyboard-read-previous", "Read: previous", Key::PageUp, Modifier::None, Command::ReadPrevious},
			Entry{"keyboard-read-next", "Read: next", Key::PageDown, Modifier::None, Command::ReadNext},
			Entry{"keyboard-read-first", "Read: previous category, or first", Key::PageUp, Modifier::Control, Command::ReadFirst},
			Entry{"keyboard-read-last", "Read: next category, or last", Key::PageDown, Modifier::Control, Command::ReadLast},
			Entry{"keyboard-compare-item", "Compare: the item in full", Key::PageUp, Modifier::Shift, Command::CompareItem},
			Entry{"keyboard-compare-equipped", "Compare: the item equipped", Key::PageDown, Modifier::Shift, Command::CompareEquipped},
			Entry{"keyboard-stash-page-previous", "Menu: shared stash, previous page", Key::PageUp, Modifier::Alt, Command::StashPagePrevious},
			Entry{"keyboard-stash-page-next", "Menu: shared stash, next page", Key::PageDown, Modifier::Alt, Command::StashPageNext},
			Entry{"keyboard-repeat", "Say it again", Key::Home, Modifier::None, Command::Repeat},
			Entry{"keyboard-read-detail", "Menu: read the description", Key::J, Modifier::None, Command::ReadDetail},
			Entry{"keyboard-swap", "Swap weapons", Key::Y, Modifier::None, Command::Swap},
			Entry{"keyboard-left-skill", "Choose the skill of the left hand", Key::S, Modifier::Shift, Command::LeftSkill},
			Entry{"keyboard-settings", "Open or close the settings list", Key::S, Modifier::Control, Command::Settings},
			Entry{"keyboard-settings-change", "Open the group or change the setting (settings list open)", Key::Space, Modifier::None, Command::SettingsChange},
			Entry{"keyboard-settings-back", "Back to the groups (settings list open)", Key::Backspace, Modifier::None, Command::SettingsBack},
			Entry{"keyboard-settings-lower", "Lower the setting (settings list open)", Key::Space, Modifier::Shift, Command::SettingsLower},
			Entry{"keyboard-click-shift", "Shift+A: attack in place in play; left click with Shift (equip) in a menu", Key::A, Modifier::Shift, Command::ClickLeft},
			Entry{"keyboard-click-control", "Menu: left click with Ctrl (sell, move, drop)", Key::A, Modifier::Control, Command::ClickLeft},
			Entry{"keyboard-click-alt", "Menu: left click with Alt", Key::A, Modifier::Alt, Command::ClickLeft},
			Entry{"keyboard-right-click-shift", "Menu: right click with Shift", Key::D, Modifier::Shift, Command::ClickRight},
			Entry{"keyboard-right-click-control", "Menu: right click with Ctrl", Key::D, Modifier::Control, Command::ClickRight},
			Entry{"keyboard-right-click-alt", "Menu: right click with Alt", Key::D, Modifier::Alt, Command::ClickRight},
			Entry{"keyboard-transmute", "Menu: transmute (Horadric Cube)", Key::T, Modifier::Alt, Command::Transmute},
			Entry{"keyboard-sort", "Menu: sort the items", Key::S, Modifier::Alt, Command::Sort},
			Entry{"keyboard-fill-belt", "Menu: fill the belt from the inventory", Key::F, Modifier::Alt, Command::FillBelt},
			Entry{"keyboard-repair", "Menu: repair", Key::R, Modifier::Shift, Command::Repair}}) {
			const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = entry.id, .displayName = entry.label, .category = "Accessibility",
				.defaultPrimary = {entry.key, entry.modifier}, .callback = OnAction,
				.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entry.command))};
			D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
			const auto result = g_input->registerAction(context, &action, &handle);
			++offered;
			if (result != D2RL::Input::Result::Success) {
				AsyncLog::WarnF(context, "[keyboard] %s was refused by the loader: result %u", entry.id, static_cast<unsigned>(result));
				continue;
			}
			++registered;
			g_actions.push_back(handle);
			// The key the loader really gave it: a saved or a clashing binding shows here.
			D2RL::Input::Binding bound{};
			if (g_input->getBinding && g_input->getBinding(context, handle, D2RL::Input::BindingSlot::Primary, &bound) == D2RL::Input::Result::Success
				&& (bound.key != entry.key || bound.modifier != entry.modifier))
				AsyncLog::WarnF(context, "[keyboard] %s is bound to key 0x%X modifier %u, not its default 0x%X modifier %u", entry.id,
					static_cast<unsigned>(bound.key), static_cast<unsigned>(bound.modifier),
					static_cast<unsigned>(entry.key), static_cast<unsigned>(entry.modifier));
		}
		AsyncLog::InfoF(context, "[keyboard] %zu of %zu keys registered; actions=%u.", registered, offered, NativeActions::Available() ? 1u : 0u);
	}
	(void)context->RegisterConsoleCommand("access-actions", Console,
		"Keyboard actions: run|plain <dx> <dy>, halt, status, log on|off, explore, retreat, stop.");
	return true;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return;
	auto command = g_command.exchange(Command::None);
	const uint64_t now = GetTickCount64();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	const bool inPlay = state.ready && RadarUi::InPlay();
	g_playing = inPlay;
	if (const char* button = g_press.exchange(nullptr); button && !KeyboardMenu::Press(context, button))
		ScreenReader::Speak(Strings::Current().unavailable, true);
	if (g_sort.exchange(false) && !KeyboardMenu::Sort(context)) ScreenReader::Speak(Strings::Current().unavailable, true);
	if (g_fill.exchange(false) && !KeyboardMenu::FillBelt(context)) ScreenReader::Speak(Strings::Current().unavailable, true);
	if (inPlay && command == Command::None) command = g_deferred.exchange(Command::None);
	if (const Command pad = PadSettings(inPlay); command == Command::None) command = pad;
	if (Settings(command, now)) command = Command::None;
	if (command == Command::LootFilter) {
		const bool on = !GroundItems::LootFilter::Active();
		GroundItems::LootFilter::SetActive(on);
		AsyncLog::InfoF(context, "[items] loot filter %s (LB)", on ? "on" : "off");
		ScreenReader::Speak(on ? Strings::Current().lootFilterOn : Strings::Current().lootFilterOff, true);
	}
	if (command == Command::Swap && state.ready) Swap(context, now);
	SwapAnswer(context, now);
	if (command == Command::LeftSkill && !(state.ready && KeyboardMenu::LeftSkillList(context)))
		ScreenReader::Speak(Strings::Current().unavailable, true);
	Read(command, inPlay);
	if (state.revision != g_revision) {
		g_revision = state.revision;
		End("lifecycle", false);
		g_lock.Clear();
		g_itemPick = 0;
		g_refused.clear();
		g_stepping = false;
		// Another level's cells: what would not come off there, and where the character last stuck.
		g_kept.clear();
		g_moveOrder = g_moveFrom = g_stuckAt = {-1, -1};
		// Where a new level is first stood in is the way in: looked at the first moment in play.
		g_nextSweep = 0;
		g_padUse.active = false;
	}
	ReleaseLostHolds();
	const bool attack = g_pressed[0].exchange(false), cast = g_pressed[1].exchange(false);
	if (command == Command::LogOn || command == Command::LogOff || command == Command::TestStatus) Test(state, command);
	if (!inPlay) {
		// A menu or a dialogue has the keyboard now. What was under way ends
		// without a word, and without a stop the menu did not ask for.
		if (g_mode != Mode::Idle) End("not in play", false);
		g_stepping = false;
		return;
	}
	if (command == Command::TestRun || command == Command::TestPlain || command == Command::TestHalt) Test(state, command);
	Sweep(state, now);
	Sent(state, now);

	// The arrows walk while held, and end whatever else was walking. While a
	// line of chat is being typed they are the text cursor's.
	const bool typing = NativeSkillNavigation::TextEditing().value_or(false);
	if (const Position heading = typing ? Position{} : Heading(); heading.x != 0 || heading.y != 0) {
		if (NativeActions::Available()) {
			if (g_mode != Mode::Idle && g_mode != Mode::Fight) End("arrow key", false);
			if (g_mode == Mode::Idle) Step(state, heading, now);
		}
	} else if (g_stepping) {
		g_stepping = false;
		(void)NativeActions::Halt();
	}

	if (attack) BeginFight(context, state, Hand::Left, now);
	else if (cast) BeginFight(context, state, Hand::Right, now);
	switch (command) {
	case Command::Interact:       BeginUse(state); break;
	case Command::TargetNext:     StepTarget(state, 1); break;
	case Command::TargetPrevious: StepTarget(state, -1); break;
	case Command::ItemNext:       StepPick(state, 1, false); break;
	case Command::ItemPrevious:   StepPick(state, -1, false); break;
	case Command::NpcNext:        StepPick(state, 1, true); break;
	case Command::NpcPrevious:    StepPick(state, -1, true); break;
	case Command::WalkToTarget:   BeginWalk(state); break;
	case Command::Explore:        BeginExplore(state); break;
	case Command::Retreat:        BeginRetreat(state); break;
	case Command::Stop:
		End("stop key", true);
		ScreenReader::Speak(Strings::Current().walkStopped, true);
		break;
	default: break;
	}

	if (g_mode == Mode::Fight && !g_held[g_hand == Hand::Left ? 0 : 1].load()) {
		// The key is up: the blow under way finishes, a walk toward the monster does not.
		End("fighting key released", g_closing || g_backing);
	}
	if (g_mode == Mode::Idle || now < g_nextAct) return;
	g_nextAct = now + ActMs;
	switch (g_mode) {
	case Mode::Walk:
	case Mode::Interact:
	case Mode::Explore:
	case Mode::Retreat: Walk(state, now); break;
	case Mode::Fight:   Fight(context, state, now); break;
	case Mode::Idle:    break;
	}
}

bool Playing() { return g_playing.load(); }
bool SettingsOpen() { return g_settingsOpen.load(); }

std::vector<bool> CutOff(const Radar::World& world, const std::vector<const EntityRadar::Entity*>& things) {
	std::vector<bool> cut(things.size(), false);
	Lifecycle state;
	state.session = world.session;
	state.level = world.level;
	const auto* reach = Reached(state, world);
	if (!reach) return cut;
	// What walking alone does not get to is cut off only when the pads do not
	// get to it either; the search through them is made only then.
	const LevelRoute::Reach* pads = nullptr;
	bool looked = false;
	for (size_t i = 0; i < things.size(); ++i) {
		if (!CutOff(*reach, state, *things[i])) continue;
		if (!looked) { pads = PadReached(state, world); looked = true; }
		LevelRoute::Pad pad;
		cut[i] = !pads || !PadWayFrom(*pads, state, *things[i], pad);
	}
	return cut;
}

void NotePads(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities) {
	Lifecycle state;
	state.session = world.session;
	state.level = world.level;
	NotePads(state, entities);
}

void NoteDoors(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities) {
	Lifecycle state;
	state.session = world.session;
	state.level = world.level;
	auto* kept = Kept(state);
	if (!kept) return;
	size_t noted = 0;
	for (const auto& entity : entities)
		if (entity.kind == EntityRadar::Kind::Object && entity.shutDoor != Generated::DoorSound::None
			&& kept->memory.NoteDoor(entity.position, std::max<int>(1, static_cast<int>(entity.doorSpan)))) ++noted;
	if (noted) AsyncLog::InfoF(g_context, "[route] shut doors noted on level %d: %zu", state.level, noted);
}

const LevelRoute::Memory* RememberedMap(const Radar::World& world) {
	Lifecycle state;
	state.session = world.session;
	state.level = world.level;
	return Remember(state, world);
}

void CancelAfterFault() noexcept {
	const bool moving = g_mode != Mode::Idle || g_stepping;
	CancelMovement(moving && g_playing.load());
	g_command = Command::None;
	g_deferred = Command::None;
	g_press = nullptr;
	g_sort = g_fill = false;
	g_swapAsked = 0;
	for (auto& held : g_held) held = 0;
	for (auto& pressed : g_pressed) pressed = false;
	g_stepping = false;
	g_padUse.active = false;
	g_lock.Clear();
	g_itemPick = 0;
}

void Unregister() {
	End("unregister", false);
	g_press = nullptr;
	g_sort = false;
	g_fill = false;
	g_swapAsked = 0;
	if (g_context && g_lifecycle) for (const auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	if (g_context && g_input) for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_listeners.clear();
	g_actions.clear();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	g_context = nullptr;
	g_lifecycle = nullptr;
	g_input = nullptr;
	g_command = Command::None;
	g_deferred = Command::None;
	for (auto& held : g_held) held = 0;
	for (auto& pressed : g_pressed) pressed = false;
	g_lock.Clear();
	g_refused.clear();
	g_graph = {};
	g_graphLevel = 0;
	g_graphSession = 0;
	g_remembered.clear();
	g_rememberedSession = 0;
	g_plan.Clear();
	g_reach = {};
	g_wayIn = {};
	g_revision = 0;
	g_stepping = false;
	g_playing = false;
	g_settingsOpen = false;
	g_settings = {};
	g_settingsBuilt = false;
}
}
