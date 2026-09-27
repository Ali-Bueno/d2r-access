#include "keyboard_service.h"

#include "async_log.h"
#include "auto_detail.h"
#include "auto_walk.h"
#include "beacon.h"
#include "beacon_navigation.h"
#include "beacon_service.h"
#include "combat_target.h"
#include "entity_cue_service.h"
#include "entity_radar.h"
#include "entity_radar_service.h"
#include "equipment_census.h"
#include "explore_goal.h"
#include "footstep_service.h"
#include "game_text_reader.h"
#include "generated/skill_attacks.h"
#include "ground_item_service.h"
#include "ground_item_text.h"
#include "input_direction.h"
#include "keyboard_menu.h"
#include "map_survey_service.h"
#include "native_actions.h"
#include "native_belt_fill.h"
#include "native_belt_rows.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_level_exits.h"
#include "native_rooms.h"
#include "native_skill_hand.h"
#include "native_skill_icons.h"
#include "native_skill_navigation.h"
#include "native_sort.h"
#include "native_swap.h"
#include "native_weapon_set.h"
#include "native_world.h"
#include "navigation_grid.h"
#include "navigation_mode.h"
#include "quest_service.h"
#include "radar_readout.h"
#include "radar_service.h"
#include "radar_ui.h"
#include "retreat.h"
#include "room_graph.h"
#include "room_service.h"
#include "screen_reader.h"
#include "settings_list.h"
#include "skill_bar_service.h"
#include "skill_navigation.h"
#include "speech_strings.h"
#include "terrain_service.h"
#include "trade_service.h"
#include "vitals_service.h"
#include "wall_slide_service.h"
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
#include <sstream>
#include <string>
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
	None, Interact, TargetNext, TargetPrevious, WalkToTarget, Explore,
	RetreatExit, RetreatBack, RetreatPassage, Stop, RoomLayout,
	ReadPrevious, ReadNext, ReadFirst, ReadLast, CompareItem, CompareEquipped, Repeat,
	Settings, SettingsChange,
	// In play and in a menu: the game's own key for it is W, the right hand's.
	Swap,
	// In play and in a menu: the list of skills for the left hand. The game's
	// own key, S, opens the right hand's and it has none for the left.
	LeftSkill,
	// The fighting keys are holds; their presses arrive through g_pressed.
	Attack, Cast,
	// In a menu only: a click with Shift or Ctrl held, and a press of a button of the menu.
	ClickLeft, ClickRight, Transmute, Sort, FillBelt,
	// Console only, for the first live tests.
	TestRun, TestPlain, TestHalt, TestStatus, LogOn, LogOff,
};
enum class Mode { Idle, Walk, Interact, Explore, Retreat, Fight };
enum class Way { Exit, Back, Passage };
enum class Progress { Going, Arrived, Blocked, NoRoute, Unreadable };

// Policy, not game constants. Distances are collision subtiles.
constexpr uint64_t ActMs = 200;       // how often a walk, a fight or a retreat looks again
constexpr uint64_t StepMs = 200;      // how often a held arrow gives its next order
constexpr uint64_t StrikeMs = 250;    // how often a held fighting key strikes again
constexpr uint64_t GearMs = 1000;     // how often the weapon in hand is looked at
constexpr double Stride = 10;         // how far ahead a held arrow aims
constexpr double UseReach = 3;        // a thing is used from this near, plus half its footprint
constexpr double UseRange = 20;       // how far D looks for something to use, in walking steps
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
// While the settings list is open the reading keys step through it and Space
// changes a switch; closed, Space is the game's.
std::atomic<bool> g_settingsOpen{false};
SettingsList::Menu g_settings;
Strings::Language g_settingsLanguage{};
bool g_settingsBuilt = false;

int g_revision = 0;
Mode g_mode = Mode::Idle;
Way g_way = Way::Exit;
Hand g_hand = Hand::Left;
Entity g_target;            // what a walk, a use or a far retreat heads for
std::string g_goalName;     // the way out a retreat last named
AutoWalk::Walker g_walker;
AutoWalk::Settings g_walk;
CombatTarget::Lock g_lock;
CombatTarget::Settings g_targets;
Retreat::Settings g_retreat;
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
	char buffer[320]{};
	std::snprintf(buffer, sizeof(buffer), format, args...);
	return buffer;
}
double Distance(Position a, Position b) { return std::hypot(a.x - b.x, a.y - b.y); }
double HalfFootprint(const Entity& entity) {
	const auto side = std::max(entity.footprint.width, entity.footprint.height);
	return side > 1 ? (side - 1) / 2.0 : 0;
}

const RoomGraph::Graph* Rooms(const Lifecycle& state) {
	if (g_graphLevel != state.level || g_graphSession != state.session || g_graph.Empty()) {
		std::vector<NativeRooms::Room> rooms;
		g_graph = NativeRooms::Read(NativeWorld::PlayerRoom(), rooms) ? RoomGraph::Build(rooms) : RoomGraph::Graph{};
		g_graphLevel = state.level;
		g_graphSession = state.session;
	}
	return g_graph.Empty() ? nullptr : &g_graph;
}

// What is around the player right now. Read fresh for every decision: the
// monsters move, and so has the player.
struct Scene {
	Radar::World world;
	Navigation::Grid grid; // the terrain layer: a monster in a doorway is not a wall
	std::vector<Entity> entities;
};
bool Look(const Lifecycle& state, Scene& scene, bool exits) {
	if (!NativeWorld::Read(state.player, state.session, state.level, scene.world)) return false;
	scene.grid = Navigation::Build(scene.world, Navigation::MaxRadius, Radar::Layer::Terrain);
	if (!NativeEntities::Read(state.player, state.session, scene.entities, exits, exits)) scene.entities.clear();
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

// A thing by name, distance and direction, as the scanner says it.
std::string Describe(const Radar::World& world, const Entity& entity, int steps) {
	const auto& strings = Strings::Current();
	const Position offset{entity.position.x - world.player.x, entity.position.y - world.player.y};
	const char* direction = strings.radarMovementDirections[static_cast<size_t>(Radar::ScreenSector(offset))];
	const double reach = steps >= 0 ? static_cast<double>(steps) : Distance(world.player, entity.position);
	const auto distance = EntityRadar::DistancePhrase(reach, EntityRadar::Settings{}, strings);
	return Format(strings.surveySteps, entity.name.c_str(), distance.c_str(), direction);
}

void End(const char* reason, bool halt) {
	if (g_mode != Mode::Idle) AsyncLog::InfoF(g_context, "[keyboard] ended: %s", reason);
	g_mode = Mode::Idle;
	g_walker.Stop();
	g_closing = g_backing = false;
	g_goalName.clear();
	if (halt) (void)NativeActions::Halt();
}

bool Ready() {
	if (NativeActions::Available()) return true;
	ScreenReader::Speak(Strings::Current().actionsUnavailable, true);
	return false;
}

// One leg of a walk toward a thing, along the route the beacon would ping.
Progress Advance(const Lifecycle& state, const Scene& scene, const Entity& target, double reach, uint64_t now) {
	if (Distance(scene.world.player, target.position) <= reach) return Progress::Arrived;
	const int margin = 1 + static_cast<int>(std::ceil(HalfFootprint(target)));
	const auto guidance = BeaconNavigation::Prepare(scene.world, target.position, Rooms(state), nullptr, margin);
	if (!guidance.available) return Progress::Unreadable;
	Beacon::Settings settings;
	settings.arrivalDistance = reach;
	const Position destination = guidance.hasWaypoint ? guidance.waypoint : target.position;
	const auto route = Beacon::FindRoute(guidance.world, destination, settings);
	if (route.path.empty()) return Progress::NoRoute;
	const auto step = g_walker.Tick(scene.world, route.path, now, g_walk);
	if (step.event == AutoWalk::Event::Blocked) return Progress::Blocked;
	if (step.event == AutoWalk::Event::Order) (void)NativeActions::MoveTo(step.order);
	return Progress::Going;
}

// ---- walking to what the scanner named, and to the nearest thing to use ----

void BeginWalk(const Lifecycle& state) {
	const auto& strings = Strings::Current();
	if (!Ready()) return;
	Entity target;
	if (!BeaconService::Offered(target, state.level)) { ScreenReader::Speak(strings.walkNothing, true); return; }
	End("walk to target", false);
	g_target = target;
	g_mode = Mode::Walk;
	g_walker.Start();
	g_nextAct = 0;
	AsyncLog::InfoF(g_context, "[keyboard] walk to id=%u at=%.0f,%.0f name=%s", target.id, target.position.x, target.position.y, target.name.c_str());
	ScreenReader::Speak(Format(strings.walkTo, target.name.c_str()), true);
}

// The thing as it is now: a unit moves, and is a unit only while its room is loaded.
bool Refresh(const Lifecycle& state, const Scene& scene, Entity& target) {
	if (target.kind == EntityRadar::Kind::Room) return true;
	if (target.kind == EntityRadar::Kind::Item) {
		GroundItems::Snapshot snapshot;
		if (!NativeGroundItems::Read(state.player, state.session, snapshot)) return true; // unread is not gone
		for (const auto& item : snapshot.items)
			if (item.id == target.id) { target.position = {item.position.x, item.position.y}; return true; }
		return false;
	}
	for (const auto& entity : scene.entities)
		if (EntityRadar::SameTarget(entity, target)) { target = entity; return true; }
	// Not loaded is not gone: a place keeps where it was.
	return target.placeId != 0 || target.unit == Entity::NoUnit;
}

bool InTargetRoom(const Lifecycle& state, Position player) {
	const auto* graph = Rooms(state);
	const size_t room = graph ? NativeRooms::Containing(graph->rooms, player) : NativeRooms::None;
	return room != NativeRooms::None && graph->rooms[room].id == g_target.placeId;
}

void Walk(const Lifecycle& state, uint64_t now) {
	const auto& strings = Strings::Current();
	Scene scene;
	if (!Look(state, scene, g_target.kind == EntityRadar::Kind::Exit)) return;
	const std::string name = g_target.name;
	if (!Refresh(state, scene, g_target)) {
		End("target gone", true);
		ScreenReader::Speak(Format(strings.beaconLost, name.c_str()), true);
		return;
	}
	const double reach = UseReach + HalfFootprint(g_target);
	if (g_mode == Mode::Explore && InTargetRoom(state, scene.world.player)) {
		End("explored", true);
		ScreenReader::Speak(strings.exploreArrived, true);
		return;
	}
	switch (Advance(state, scene, g_target, g_mode == Mode::Explore ? 0 : reach, now)) {
	case Progress::Arrived:
		if (g_mode == Mode::Interact) {
			AsyncLog::InfoF(g_context, "[keyboard] use id=%u unit=%u name=%s", g_target.id, g_target.unit, name.c_str());
			End("used", false);
			(void)NativeActions::Interact(static_cast<Unit>(g_target.unit), g_target.id);
		} else {
			End("arrived", true);
			ScreenReader::Speak(Format(strings.beaconArrived, name.c_str()), true);
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

void BeginUse(const Lifecycle& state) {
	const auto& strings = Strings::Current();
	if (!Ready()) return;
	Scene scene;
	if (!Look(state, scene, false)) { ScreenReader::Speak(strings.interactNothing, true); return; }
	std::vector<Entity> things;
	for (const auto& entity : scene.entities) {
		using EntityRadar::Kind;
		if (entity.unit == Entity::NoUnit) continue;
		if (entity.kind == Kind::Npc || entity.kind == Kind::Object || entity.kind == Kind::Exit) things.push_back(entity);
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
	for (const auto& thing : things) {
		const int steps = Steps(scene.grid, thing.position, 1 + static_cast<int>(std::ceil(HalfFootprint(thing))));
		if (steps < 0 || steps > UseRange || (nearest && steps >= fewest)) continue;
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

void BeginExplore(const Lifecycle& state) {
	const auto& strings = Strings::Current();
	if (!Ready()) return;
	Scene scene;
	const auto* graph = Rooms(state);
	if (!graph || !Look(state, scene, false)) { ScreenReader::Speak(strings.terrainUnavailable, true); return; }
	const auto explored = TerrainService::ExploredRooms(state.level, graph->rooms, scene.world.player);
	const auto choice = ExploreGoal::Nearest(*graph, scene.grid, explored, scene.world.player, g_refused);
	if (!choice.found) {
		AsyncLog::InfoF(g_context, "[keyboard] explore: nothing left, %zu rooms, %zu explored, %zu given up", graph->rooms.size(), explored.size(), g_refused.size());
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

void BeginRetreat(const Lifecycle&, Way way) {
	if (!Ready()) return;
	End("retreat", false);
	g_way = way;
	g_mode = Mode::Retreat;
	g_walker.Start();
	g_nextAct = 0;
}

void RetreatOn(const Lifecycle& state, uint64_t now) {
	const auto& strings = Strings::Current();
	Scene scene;
	if (!Look(state, scene, g_way != Way::Passage)) return;
	std::vector<Position> goals, threats;
	std::vector<const Entity*> ways;
	for (const auto& entity : scene.entities) {
		if (entity.kind == EntityRadar::Kind::Enemy) threats.push_back(entity.position);
		if (entity.kind != EntityRadar::Kind::Exit || g_way == Way::Passage) continue;
		if (g_way == Way::Back && static_cast<int>(entity.destinationLevel) != state.cameFrom) continue;
		goals.push_back(entity.position);
		ways.push_back(&entity);
	}
	auto settings = g_retreat;
	if (g_way == Way::Passage) {
		goals = Retreat::Chokepoints(scene.grid);
		settings.arrival = 1;
	}
	if (goals.empty()) {
		End("no goal", false);
		ScreenReader::Speak(g_way == Way::Passage ? strings.retreatNoPassage
			: g_way == Way::Back ? strings.retreatBackUnknown : strings.retreatNone, true);
		return;
	}
	const auto plan = Retreat::Find(scene.grid, goals, threats, settings);
	if (!plan.found) {
		if (g_way == Way::Passage) {
			End("no passage reached", false);
			ScreenReader::Speak(strings.retreatNoPassage, true);
			return;
		}
		// Every way out lies past what the game keeps loaded around the player:
		// head for the nearest, room by room, as a walk to it would.
		const Entity* nearest = ways.front();
		for (const auto* way : ways)
			if (Distance(scene.world.player, way->position) < Distance(scene.world.player, nearest->position)) nearest = way;
		g_target = *nearest;
		g_mode = Mode::Walk;
		g_walker.Start();
		AsyncLog::InfoF(g_context, "[keyboard] retreat: far way out id=%u name=%s", g_target.id, g_target.name.c_str());
		ScreenReader::Speak(Format(strings.retreatTo, g_target.name.c_str()), true);
		return;
	}
	const std::string name = g_way == Way::Passage ? std::string(strings.roomOpening) : ways[plan.goal]->name;
	if (name != g_goalName) {
		// The first choice, or a better one now that the monsters have moved.
		const bool first = g_goalName.empty();
		g_goalName = name;
		AsyncLog::InfoF(g_context, "[keyboard] retreat: to %s, %zu cells, threatened=%u, %zu monsters", name.c_str(), plan.path.size(), plan.threatened ? 1u : 0u, threats.size());
		ScreenReader::Speak(Format(strings.retreatTo, name.c_str()), first);
	}
	if (plan.path.size() <= 1) {
		End("retreat arrived", true);
		ScreenReader::Speak(Format(strings.beaconArrived, name.c_str()), true);
		return;
	}
	const auto step = g_walker.Tick(scene.world, plan.path, now, g_walk);
	if (step.event == AutoWalk::Event::Blocked) {
		End("retreat blocked", true);
		ScreenReader::Speak(Format(strings.walkBlocked, name.c_str()), true);
	} else if (step.event == AutoWalk::Event::Order) {
		(void)NativeActions::MoveTo(step.order);
	}
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
	return CombatTarget::Rank(scene.world, scene.grid, scene.entities, g_targets);
}

void StepTarget(const Lifecycle& state, int delta) {
	const auto& strings = Strings::Current();
	Scene scene;
	if (!Look(state, scene, false)) { ScreenReader::Speak(strings.targetNone, true); return; }
	const auto ranked = Monsters(scene);
	const auto* target = g_lock.Choose(ranked, delta);
	if (!target) { ScreenReader::Speak(strings.targetNone, true); return; }
	AsyncLog::InfoF(g_context, "[keyboard] target id=%u steps=%d sight=%u of %zu name=%s", target->entity.id, target->steps,
		target->sight ? 1u : 0u, ranked.size(), target->entity.name.c_str());
	ScreenReader::Speak(Describe(scene.world, target->entity, target->steps), true);
}

void BeginFight(const D2RL::PluginContext* context, const Lifecycle& state, Hand hand, uint64_t now) {
	if (!Ready()) return;
	const int skill = NativeActions::SelectedSkill(hand);
	if (KindOf(hand) == Generated::SkillAttack::None) {
		// Not an attack: a cry, an armour, a summons. Used once, where the
		// monster held stands when there is one, where the player stands otherwise.
		Scene scene;
		if (!Look(state, scene, false)) return;
		bool moved = false;
		const auto ranked = Monsters(scene);
		const auto* target = g_lock.Held() ? g_lock.Keep(ranked, moved) : nullptr;
		AsyncLog::InfoF(g_context, "[keyboard] skill %d is not an attack: used once", skill);
		(void)NativeActions::SkillAt(hand, target ? target->entity.position : scene.world.player);
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
	Scene scene;
	if (!Look(state, scene, false)) return;
	const auto ranked = Monsters(scene);
	bool moved = false;
	const auto* target = g_lock.Keep(ranked, moved);
	if (!target) {
		if (!g_saidNone) ScreenReader::Speak(strings.targetNone, true);
		g_saidNone = true;
		if (g_closing || g_backing) { g_closing = g_backing = false; (void)NativeActions::Halt(); }
		return;
	}
	g_saidNone = false;
	if (moved) {
		// A monster is named once, when the fight turns to it.
		AsyncLog::InfoF(g_context, "[keyboard] fight: now id=%u name=%s", target->entity.id, target->entity.name.c_str());
		ScreenReader::Speak(target->entity.name, false);
		g_walker.Start();
	}
	const auto reach = ReachOf(context, g_hand, now);
	if (reach.ranged) {
		// A ranged fight keeps its distance: a shot, a fall back when a monster
		// closes, the next shot from there. The order is given again until the
		// character is free to move, since a shot under way cannot be broken off.
		if (g_backing) {
			if (Distance(scene.world.player, g_backTo) > 2 && now - g_backStart < BackMs) {
				(void)NativeActions::MoveTo(g_backTo);
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
				(void)NativeActions::MoveTo(g_backTo);
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
			(void)NativeActions::MoveTo(step.order);
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
		(void)NativeActions::MoveTo(aim);
		return;
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

SettingsList::Menu BuildSettings() {
	using SettingsList::Entry;
	using Cue = EntityCueService::Cue;
	using Item = GroundItemService::Switch;
	using T = Strings::Table;
	const auto phrase = [](const char* T::* on, const char* T::* off) {
		return [on, off](bool state) { return std::string(Strings::Current().*(state ? on : off)); };
	};
	std::vector<Entry> entries{
		{AutoDetail::Enabled, AutoDetail::SetEnabled, phrase(&T::autoDetailEnabled, &T::autoDetailDisabled), true},
		{AutoDetail::ControlTooltips, AutoDetail::SetControlTooltips, phrase(&T::controlTooltipsEnabled, &T::controlTooltipsDisabled), true},
		{[] { return RadarService::Enabled(); }, [](bool on) { RadarService::SetEnabled(on); }, phrase(&T::radarEnabled, &T::radarDisabled), true},
		{RadarService::Diagonal, RadarService::SetDiagonal, [](bool diagonal) {
			return RadarReadout::DirectionList(diagonal ? Radar::Frame::Diagonal : Radar::Frame::Cardinal, Strings::Current()); }, true},
		{NavigationMode::FullAssistance, BeaconService::SetFullAssistance, phrase(&T::navigationFullAssistance, &T::navigationExploration), true},
		{[] { return RoomService::Enabled(); }, [](bool on) { RoomService::SetEnabled(on); }, phrase(&T::roomsEnabled, &T::roomsDisabled), true},
		{RoomService::Finder, RoomService::SetFinder, phrase(&T::finderEnabled, &T::finderDisabled), true},
		{EntityRadarService::NpcEnabled, EntityRadarService::SetNpcEnabled, phrase(&T::npcEnabled, &T::npcDisabled), true},
		{[] { return EntityCueService::Enabled(Cue::All); }, [](bool on) { EntityCueService::SetEnabled(Cue::All, on); }, phrase(&T::entityCuesEnabled, &T::entityCuesDisabled), true},
		{[] { return EntityCueService::Enabled(Cue::Npcs); }, [](bool on) { EntityCueService::SetEnabled(Cue::Npcs, on); }, phrase(&T::entityCueNpcsEnabled, &T::entityCueNpcsDisabled), true},
		{[] { return EntityCueService::Enabled(Cue::Enemies); }, [](bool on) { EntityCueService::SetEnabled(Cue::Enemies, on); }, phrase(&T::entityCueEnemiesEnabled, &T::entityCueEnemiesDisabled), true},
		{[] { return EntityCueService::Enabled(Cue::Ranged); }, [](bool on) { EntityCueService::SetEnabled(Cue::Ranged, on); }, phrase(&T::entityCueRangedEnabled, &T::entityCueRangedDisabled), true},
		{[] { return GroundItemService::Enabled(Item::Speech); }, [](bool on) { GroundItemService::SetEnabled(Item::Speech, on); }, [](bool on) { return std::string(GroundItemText::Enabled(on)); }, true},
		{[] { return GroundItemService::Enabled(Item::Sounds); }, [](bool on) { GroundItemService::SetEnabled(Item::Sounds, on); }, [](bool on) { return std::string(GroundItemText::SoundEnabled(on)); }, true},
		{[] { return GroundItemService::Enabled(Item::Exits); }, [](bool on) { GroundItemService::SetEnabled(Item::Exits, on); }, [](bool on) { return std::string(GroundItemText::ExitSoundEnabled(on)); }, true},
		{[] { return GroundItemService::Enabled(Item::Chests); }, [](bool on) { GroundItemService::SetEnabled(Item::Chests, on); }, [](bool on) { return std::string(GroundItemText::ChestSoundEnabled(on)); }, true},
		{[] { return GroundItemService::Enabled(Item::Doors); }, [](bool on) { GroundItemService::SetEnabled(Item::Doors, on); }, [](bool on) { return std::string(GroundItemText::DoorSoundEnabled(on)); }, true},
		{[] { return FootstepService::Enabled(); }, [](bool on) { FootstepService::SetEnabled(on); }, phrase(&T::footstepsEnabled, &T::footstepsDisabled), true},
		{[] { return WallSlideService::Enabled(); }, [](bool on) { WallSlideService::SetEnabled(on); }, phrase(&T::movementEnabled, &T::movementDisabled), true},
		{[] { return VitalsService::Enabled(); }, [](bool on) { VitalsService::SetEnabled(on); }, phrase(&T::vitalsEnabled, &T::vitalsDisabled), true},
		{[] { return QuestService::Enabled(); }, [](bool on) { QuestService::SetEnabled(on); }, phrase(&T::questAlertsEnabled, &T::questAlertsDisabled), true},
		{GameTextReader::Enabled, GameTextReader::SetEnabled, phrase(&T::gameTextEnabled, &T::gameTextDisabled), true},
		{[] { return SkillBarService::Enabled(); }, [](bool on) { SkillBarService::SetEnabled(on); }, phrase(&T::skillBarReaderOn, &T::skillBarReaderOff), false},
		{[] { return TradeService::Enabled(); }, [](bool on) { TradeService::SetEnabled(on); }, phrase(&T::tradeAlertsOn, &T::tradeAlertsOff), false},
		{[] { return WeaponSwapService::Enabled(); }, [](bool on) { WeaponSwapService::SetEnabled(on); }, phrase(&T::weaponSwapAlertsOn, &T::weaponSwapAlertsOff), false},
		{SkillNavigation::Enabled, SkillNavigation::SetEnabled, phrase(&T::skillGridOn, &T::skillGridOff), false},
		{TerrainService::RoomSounds, TerrainService::SetRoomSounds, phrase(&T::explorationSoundsOn, &T::explorationSoundsOff), false},
		{TerrainService::PlaceAlerts, TerrainService::SetPlaceAlerts, phrase(&T::discoveriesOn, &T::discoveriesOff), false},
	};
	return SettingsList::Menu(std::move(entries), Strings::Current().settingsOpened, Strings::Current().settingsClosed);
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
	if (command == Command::Settings) text = g_settings.Toggle(now);
	else if (!g_settings.Open()) spent = false;
	else if (command == Command::SettingsChange) text = g_settings.Change(now);
	else if (command == Command::ReadPrevious || command == Command::ReadFirst) text = g_settings.Step(-1, now);
	else if (command == Command::ReadNext || command == Command::ReadLast) text = g_settings.Step(1, now);
	else spent = false;
	if (!spent) text = g_settings.Expire(now, SettingsIdleMs);
	g_settingsOpen = g_settings.Open();
	if (!text.empty()) {
		AsyncLog::InfoF(g_context, "[keyboard] settings: %s", text.c_str());
		ScreenReader::Speak(text, true);
	}
	return spent;
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
	else if (word == "retreat") g_deferred = Command::RetreatExit;
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
	if (command == Command::SettingsChange && !g_settingsOpen.load()) return D2RL::Input::ActionResult::Ignored;
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
		const char* button = command == Command::RetreatExit ? RepairAllButton : command == Command::RetreatBack ? RepairButton
			: command == Command::Transmute ? TransmuteButton : nullptr;
		const bool menuKey = button || command == Command::ClickLeft || command == Command::ClickRight
			|| command == Command::ReadPrevious || command == Command::ReadNext || command == Command::Repeat || command == Command::Stop;
		if (menuKey || command == Command::Sort || command == Command::FillBelt) {
			if (!pressed) return D2RL::Input::ActionResult::Ignored;
			if (command == Command::Sort) g_sort = true;
			else if (command == Command::FillBelt) g_fill = true;
			else if (button) g_press = button;
			else KeyboardMenu::Deliver(command == Command::ClickLeft ? Act::ClickLeft : command == Command::ClickRight ? Act::ClickRight
				: command == Command::ReadPrevious ? Act::TabPrevious : command == Command::ReadNext ? Act::TabNext
				: command == Command::Repeat ? Act::MenuPrevious : Act::MenuNext);
			return D2RL::Input::ActionResult::Handled;
		}
	}
	// Out of a menu a click with Shift or Ctrl, and the cube's key, are the game's keys.
	if (command == Command::ClickLeft || command == Command::ClickRight || command == Command::Transmute || command == Command::Sort
		|| command == Command::FillBelt)
		return D2RL::Input::ActionResult::Ignored;
	// A letter typed into a line of chat is the chat's.
	if ((command == Command::Swap || command == Command::LeftSkill) && NativeSkillNavigation::TextEditing().value_or(false))
		return D2RL::Input::ActionResult::Ignored;
	const bool reading = command == Command::Swap || command == Command::LeftSkill
		|| command == Command::ReadPrevious || command == Command::ReadNext || command == Command::ReadFirst
		|| command == Command::ReadLast || command == Command::CompareItem || command == Command::CompareEquipped
		|| command == Command::Repeat || command == Command::Settings || command == Command::SettingsChange;
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
	(void)NativeActions::Initialize(context, nativeHooks);
	if (nativeHooks) (void)NativeSort::Initialize(context);
	if (nativeHooks) (void)NativeSwap::Initialize(context);
	if (nativeHooks) (void)NativeBeltRows::Initialize(context);
	if (nativeHooks) (void)NativeBeltFill::Initialize(context);
	if (nativeHooks) (void)NativeSkillHand::Initialize(context);
	if (nativeHooks) (void)NativeSkillIcons::Initialize(context);
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
			Entry{"keyboard-use", "Walk up to the nearest thing and use it; in a menu, right click", Key::D, Modifier::None, Command::Interact},
			Entry{"keyboard-walk", "Walk to the scanned target", Key::Home, Modifier::Shift, Command::WalkToTarget},
			Entry{"keyboard-explore", "Walk to unexplored ground", Key::H, Modifier::None, Command::Explore},
			Entry{"keyboard-retreat", "Retreat to the nearest way out", Key::R, Modifier::Control, Command::RetreatExit},
			Entry{"keyboard-retreat-back", "Retreat the way you came in", Key::R, Modifier::Shift, Command::RetreatBack},
			Entry{"keyboard-retreat-passage", "Retreat to a corridor or doorway", Key::R, Modifier::Alt, Command::RetreatPassage},
			Entry{"keyboard-stop", "Stop walking or fighting", Key::End, Modifier::None, Command::Stop},
			Entry{"keyboard-room", "Say the ways out of this room", Key::Y, Modifier::Control, Command::RoomLayout},
			Entry{"keyboard-read-previous", "Read: previous", Key::PageUp, Modifier::None, Command::ReadPrevious},
			Entry{"keyboard-read-next", "Read: next", Key::PageDown, Modifier::None, Command::ReadNext},
			Entry{"keyboard-read-first", "Read: previous category, or first", Key::PageUp, Modifier::Control, Command::ReadFirst},
			Entry{"keyboard-read-last", "Read: next category, or last", Key::PageDown, Modifier::Control, Command::ReadLast},
			Entry{"keyboard-compare-item", "Compare: the item in full", Key::PageUp, Modifier::Shift, Command::CompareItem},
			Entry{"keyboard-compare-equipped", "Compare: the item equipped", Key::PageDown, Modifier::Shift, Command::CompareEquipped},
			Entry{"keyboard-repeat", "Say it again", Key::Home, Modifier::None, Command::Repeat},
			Entry{"keyboard-swap", "Swap weapons", Key::Y, Modifier::None, Command::Swap},
			Entry{"keyboard-left-skill", "Choose the skill of the left hand", Key::S, Modifier::Shift, Command::LeftSkill},
			Entry{"keyboard-settings", "Open or close the settings list", Key::S, Modifier::Control, Command::Settings},
			Entry{"keyboard-settings-change", "Change the setting (settings list open)", Key::Space, Modifier::None, Command::SettingsChange},
			Entry{"keyboard-click-shift", "Menu: left click with Shift (equip)", Key::A, Modifier::Shift, Command::ClickLeft},
			Entry{"keyboard-click-control", "Menu: left click with Ctrl (sell, move, drop)", Key::A, Modifier::Control, Command::ClickLeft},
			Entry{"keyboard-right-click-shift", "Menu: right click with Shift", Key::D, Modifier::Shift, Command::ClickRight},
			Entry{"keyboard-transmute", "Menu: transmute (Horadric Cube)", Key::T, Modifier::Alt, Command::Transmute},
			Entry{"keyboard-sort", "Menu: sort the items", Key::S, Modifier::Alt, Command::Sort},
			Entry{"keyboard-fill-belt", "Menu: fill the belt from the inventory", Key::F, Modifier::Alt, Command::FillBelt}}) {
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
	if (Settings(command, now)) command = Command::None;
	if (command == Command::Swap && state.ready) Swap(context, now);
	SwapAnswer(context, now);
	if (command == Command::LeftSkill && !(state.ready && KeyboardMenu::LeftSkillList(context)))
		ScreenReader::Speak(Strings::Current().unavailable, true);
	Read(command, inPlay);
	if (state.revision != g_revision) {
		g_revision = state.revision;
		End("lifecycle", false);
		g_lock.Clear();
		g_refused.clear();
		g_stepping = false;
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
	case Command::WalkToTarget:   BeginWalk(state); break;
	case Command::Explore:        BeginExplore(state); break;
	case Command::RetreatExit:    BeginRetreat(state, Way::Exit); break;
	case Command::RetreatBack:    BeginRetreat(state, Way::Back); break;
	case Command::RetreatPassage: BeginRetreat(state, Way::Passage); break;
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
	case Mode::Explore: Walk(state, now); break;
	case Mode::Retreat: RetreatOn(state, now); break;
	case Mode::Fight:   Fight(context, state, now); break;
	case Mode::Idle:    break;
	}
}

bool Playing() { return g_playing.load(); }
bool SettingsOpen() { return g_settingsOpen.load(); }

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
	g_revision = 0;
	g_stepping = false;
	g_playing = false;
	g_settingsOpen = false;
	g_settings = {};
	g_settingsBuilt = false;
}
}
