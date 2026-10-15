#include "entity_cues.h"
#include "test_support.h"

#include <cmath>
#include <limits>
#include <map>
#include <set>

using namespace D2RAccess;
using namespace EntityCues;
namespace {
constexpr int Size = 60;
Radar::World World() {
	Radar::World world;
	world.player = {30, 30}; world.walkingMask = 1;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size, 0)});
	return world;
}
EntityRadar::Entity Make(uint32_t id, Kind kind, double x, double y) {
	EntityRadar::Entity e;
	e.id = id; e.position = {x, y};
	e.kind = kind == Kind::Npc ? EntityRadar::Kind::Npc : EntityRadar::Kind::Enemy;
	e.enemy.important = kind == Kind::Important;
	e.enemy.resurrector = kind == Kind::Resurrector;
	e.enemy.ranged = kind == Kind::Ranged;
	return e;
}
bool Is(const std::optional<Cue>& cue, uint32_t id) { return cue && cue->id == id; }
void FilteringAndPriority() {
	const auto world = World();
	auto melee = Make(1, Kind::Ranged, 31, 30); melee.enemy = {};
	auto object = Make(2, Kind::Important, 31, 30); object.kind = EntityRadar::Kind::Object;
	Check(Select(world, {object}, {}).empty(), "objects have no cue even with hostile-looking flags");
	Settings threeVoices; threeVoices.maxVoices = 3;
	const auto ordinary = Select(world, {Make(9, Kind::Ordinary, 35, 30), melee, Make(10, Kind::Ordinary, 30, 34),
		Make(11, Kind::Ordinary, 30, 33), Make(12, Kind::Ordinary, 26, 30)}, threeVoices);
	Check(ordinary.size() == 3 && ordinary[0].id == 1 && ordinary[1].id == 11 && ordinary[2].id == 10
		&& ordinary[0].kind == Kind::Ordinary,
		"the nearest ordinary enemies of a pack each sound, nearest first, as in Diablo 1");
	Check(ordinary[0].slot != ordinary[1].slot && ordinary[1].slot != ordinary[2].slot,
		"each rank keeps a cadence of its own");
	auto blocked = World();
	for (int x = 0; x < Size; ++x) blocked.rooms[0].cells[32 * Size + x] = 1;
	const auto seen = Select(blocked, {Make(11, Kind::Ordinary, 30, 33), Make(12, Kind::Ordinary, 26, 30)}, {});
	Check(seen.size() == 1 && seen[0].id == 12 && seen[0].slot == static_cast<size_t>(Kind::Ordinary),
		"a nearer enemy behind a wall gives its rank to the next one in sight");
	Settings quiet; quiet.ordinary = false;
	Check(Select(world, {melee}, quiet).empty(), "ordinary enemy cues can be disabled separately");
	const std::vector entities{Make(3, Kind::Npc, 32, 30), Make(4, Kind::Npc, 34, 30),
		Make(5, Kind::Important, 38, 30), Make(6, Kind::Resurrector, 36, 30), Make(7, Kind::Ranged, 31, 30),
		Make(8, Kind::Ordinary, 31, 31)};
	const auto selected = Select(world, entities, threeVoices);
	Check(selected.size() == 4 && selected[0].id == 5 && selected[1].id == 6 && selected[2].id == 7 && selected[3].id == 3,
		"three priority enemy classes fill the enemy slots before the ordinary one, with one nearest NPC independently");
	const auto mixed = Select(world, {entities[2], entities[3], entities[5], Make(13, Kind::Ordinary, 32, 32)}, threeVoices);
	Check(mixed.size() == 3 && mixed[2].id == 8 && mixed[2].kind == Kind::Ordinary,
		"beside a boss and a shaman only the nearest ordinary enemy fits in the three streams");
	Check(Select(world, {entities[0], entities[1]}, {}).size() == 1, "a town never creates one stream per NPC");
	Check(Select(world, {Make(8, Kind::Ranged, 32, 30), Make(9, Kind::Ranged, 34, 30)}, {}).size() == 1,
		"ranged packs produce only their nearest representative");
	Settings settings; settings.ranged = false;
	Check(Select(world, {entities[4]}, settings)[0].kind == Kind::Ordinary,
		"an ordinary ranged enemy without its own cue still sounds as an ordinary enemy");
	settings.ordinary = false;
	Check(Select(world, {entities[4]}, settings).empty(), "ordinary ranged enemies can be disabled separately");
	auto eliteArcher = entities[4]; eliteArcher.enemy.important = true;
	Check(Select(world, {eliteArcher}, settings)[0].kind == Kind::Important, "disabling ordinary ranged cues preserves elite archers");
	settings.npcs = false; settings.enemies = false;
	Check(Select(world, entities, settings).empty(), "both group switches silence all cues");
	const auto ties = Select(world, {Make(20, Kind::Npc, 30, 32), Make(10, Kind::Npc, 32, 30)}, {});
	Check(ties.size() == 1 && ties[0].id == 10, "distance ties use stable identity independent of list order");
}
void VisibilityAndPlacement() {
	auto world = World();
	const auto npc = Make(1, Kind::Npc, 30, 38);
	for (int x = 0; x < Size; ++x) world.rooms[0].cells[34 * Size + x] = 1;
	Check(Select(world, {npc}, {}).empty(), "known walls hide cues like the directional radar");
	world = World();
	Settings circle; circle.onScreen = false;
	const auto edge = Select(world, {Make(2, Kind::Npc, 45, 30)}, circle);
	Check(edge.size() == 1 && edge[0].intervalMs == Settings{}.farIntervalMs, "range boundary has the slowest pulse");
	Check(Select(world, {Make(2, Kind::Npc, 45.01, 30)}, circle).empty(), "outside proximity range stays silent");
	const auto here = Select(world, {Make(3, Kind::Npc, 30, 30)}, {});
	Check(here.size() == 1 && here[0].right == 0 && here[0].front == 1
		&& here[0].intervalMs == Settings{}.nearIntervalMs, "coincident positions are centered without division by zero");
	const auto up = Select(world, {Make(1, Kind::Npc, 25, 25)}, {})[0];
	const auto down = Select(world, {Make(1, Kind::Npc, 35, 35)}, {})[0];
	const auto left = Select(world, {Make(1, Kind::Npc, 25, 35)}, {})[0];
	const auto right = Select(world, {Make(1, Kind::Npc, 35, 25)}, {})[0];
	Check(std::abs(up.right) < 0.001 && up.front > 0.999 && down.front < -0.999, "front and rear follow fixed screen axes");
	Check(left.right < -0.999 && right.right > 0.999 && std::abs(left.front) < 0.001, "left and right follow fixed screen axes");
	const double nan = std::numeric_limits<double>::quiet_NaN();
	Check(Select(world, {Make(4, Kind::Npc, nan, 30)}, {}).empty(), "invalid positions never reach terrain ray or audio");
	Settings invalid; invalid.range = nan;
	Check(Select(world, {npc}, invalid).empty(), "invalid range stays silent");
	world.player.x = nan;
	Check(Select(world, {npc}, {}).empty(), "invalid player position stays silent");
}
void CadenceAndLifecycle() {
	const auto world = World();
	const auto far = Make(1, Kind::Npc, 45, 30), near = Make(1, Kind::Npc, 30, 30);
	Tracker tracker;
	Check(Is(tracker.Update(world, {far}, 0, {}), 1), "new target starts immediately at time zero");
	Check(!tracker.Update(world, {far}, 300, {}), "distant target waits longer than a near target");
	Check(Is(tracker.Update(world, {near}, 300, {}), 1), "approaching recalculates cadence without waiting for old far deadline");
	Check(!tracker.Update(world, {near}, 449, {}), "near pulse respects the minimum interval");
	Check(Is(tracker.Update(world, {near}, 450, {}), 1), "near pulse repeats at the shorter interval");
	Check(!tracker.Update(world, {Make(2, Kind::Npc, 30, 30)}, 550, {}), "nearest target changes do not burst pulses");
	Check(!tracker.Update(world, {}, 600, {}), "vanished or dead targets cannot repeat from memory");
	Check(Is(tracker.Update(world, {near}, 600, {}), 1), "a new visible target starts once after disappearance");
	tracker.Reset();
	Check(Is(tracker.Update(world, {far}, 2150, {}), 1), "lifecycle reset discards previous timers");
	Check(Is(tracker.Update(world, {far}, 10, {}), 1), "a backwards clock resets instead of underflowing");
}
void FairScheduling() {
	const auto world = World();
	const std::vector entities{Make(1, Kind::Important, 30, 30), Make(2, Kind::Resurrector, 30, 30), Make(3, Kind::Npc, 30, 30)};
	Tracker tracker;
	std::map<uint32_t, unsigned> counts;
	uint64_t previous = 0;
	bool first = true;
	for (uint64_t now = 0; now < 6000; now += 50) {
		const auto cue = tracker.Update(world, entities, now, {});
		if (!cue) continue;
		Check(first || now - previous >= Settings{}.gapMs, "different categories leave an audible gap");
		++counts[cue->id]; previous = now; first = false;
	}
	Check(counts.size() == 3 && counts[1] >= 18 && counts[2] >= 18 && counts[3] >= 18,
		"even three simultaneous classes each repeat quickly without starvation");
	tracker.Reset();
	Check(Is(tracker.Update(world, entities, 0, {}), 1), "first cue prefers important enemies");
	Check(!tracker.Update(world, entities, 99, {}), "simultaneously discovered targets are staggered");
	Check(Is(tracker.Update(world, entities, 100, {}), 2), "shaman follows the important enemy");
}
void PackCadence() {
	const auto world = World();
	const std::vector pack{Make(1, Kind::Ordinary, 31, 30), Make(2, Kind::Ordinary, 30, 32), Make(3, Kind::Ordinary, 28, 30)};
	Tracker tracker;
	std::map<uint32_t, unsigned> counts;
	for (uint64_t now = 0; now < 3000; now += 100)
		if (const auto cue = tracker.Update(world, pack, now, {})) ++counts[cue->id];
	Check(counts.size() == 3 && counts[1] >= 8 && counts[2] >= 8 && counts[3] >= 8,
		"three nearby pack members each repeat, none starved");
}
void FastRepeatsAtRuntimeCadence() {
	const auto world = World();
	for (const auto kind : {Kind::Npc, Kind::Important, Kind::Resurrector, Kind::Ranged, Kind::Ordinary}) {
		Tracker near, far;
		unsigned nearCount = 0, farCount = 0;
		// The service reads a fresh snapshot every 100 ms; measure audible starts
		// at that cadence, including the rounding of the configured intervals.
		for (uint64_t now = 0; now < 3000; now += 100) {
			if (near.Update(world, {Make(1, kind, 30, 30)}, now, {})) ++nearCount;
			if (far.Update(world, {Make(1, kind, 45, 30)}, now, {})) ++farCount;
		}
		Check(nearCount >= 14 && farCount >= 6 && nearCount > farCount,
			"NPC and every enemy class repeat much faster at the real polling cadence");
	}
}
}
void GrowlsOff() {
	const auto world = World();
	Settings quiet; quiet.ordinary = false;
	const std::vector pack{Make(1, Kind::Ordinary, 31, 30), Make(2, Kind::Ordinary, 30, 33), Make(3, Kind::Ordinary, 30, 34)};
	Check(Select(world, pack, quiet).empty(), "with growls off, ordinary packs stay silent");
	const std::vector entities{Make(3, Kind::Npc, 32, 30), Make(5, Kind::Important, 38, 30),
		Make(6, Kind::Resurrector, 36, 30), Make(7, Kind::Ranged, 31, 30)};
	const auto selected = Select(world, entities, quiet);
	Check(selected.size() == 3 && selected[0].id == 5 && selected[1].id == 6 && selected[2].id == 3,
		"with growls off, boss and shaman take the two enemy slots, with one nearest NPC independently");
}
void ScreenArea() {
	Radar::World world;
	world.player = {100, 100}; world.walkingMask = 1;
	world.rooms.push_back({0, 0, 200, 200, std::vector<uint16_t>(200 * 200, 0)});
	const Settings screen; // on_screen is the default
	Settings circle; circle.onScreen = false;
	Check(Select(world, {Make(1, Kind::Ordinary, 120, 100)}, screen).size() == 1,
		"an enemy 20 subtiles off, past the old 15-subtile circle, growls once on screen");
	Check(Select(world, {Make(1, Kind::Ordinary, 100, 130)}, screen).size() == 1, "straight down 30 is still on screen");
	Check(Select(world, {Make(1, Kind::Ordinary, 100, 140)}, screen).empty(), "past the bottom of the screen stays silent");
	Check(Select(world, {Make(1, Kind::Ordinary, 100 + 40, 100)}, screen).empty(), "past the right of the screen stays silent");
	// Diagonal offset t: |dx+dy| = 2t, so the plain screen ends at 18.5 and the ranged margin at 20.
	Check(Select(world, {Make(2, Kind::Ordinary, 119, 119)}, screen).empty(), "an ordinary enemy just past the screen edge is silent");
	Check(Select(world, {Make(3, Kind::Ranged, 119, 119)}, screen).size() == 1, "an archer just past the screen edge already sounds");
	Check(Select(world, {Make(3, Kind::Ranged, 121, 121)}, screen).empty(), "the archer margin is small");
	Check(Select(world, {Make(1, Kind::Ordinary, 120, 100)}, circle).empty() && Select(world, {Make(1, Kind::Ordinary, 114, 100)}, circle).size() == 1,
		"with on_screen off the range circle applies again");
	auto walled = world;
	for (int x = 0; x < 200; ++x) walled.rooms[0].cells[110 * 200 + x] = 1;
	Check(Select(walled, {Make(1, Kind::Ordinary, 100, 125)}, screen).empty(), "line of sight still hides an on-screen enemy");
	const auto far = Select(world, {Make(1, Kind::Ordinary, 100, 130)}, screen)[0];
	const auto near = Select(world, {Make(1, Kind::Ordinary, 100, 101)}, screen)[0];
	Check(far.intervalMs > near.intervalMs && far.intervalMs < screen.farIntervalMs,
		"pacing is scaled over the screen extent, so a far enemy is slower but not at the maximum");
}
void BigPack() {
	Radar::World world;
	world.player = {100, 100}; world.walkingMask = 1;
	world.rooms.push_back({0, 0, 200, 200, std::vector<uint16_t>(200 * 200, 0)});
	std::vector<EntityRadar::Entity> pack;
	for (uint32_t i = 0; i < 20; ++i) pack.push_back(Make(100 + i, Kind::Ordinary, 100 + 2 + static_cast<double>(i), 100));
	pack.push_back(Make(1, Kind::Important, 100, 118));
	for (const size_t voices : {size_t{1}, size_t{3}, size_t{6}, MaxVoices}) {
		Settings settings; settings.maxVoices = voices;
		const auto selected = Select(world, pack, settings);
		Check(selected.size() == voices, "a big pack fills exactly the configured voices");
		Check(selected[0].id == 1 && selected[0].kind == Kind::Important, "the elite always keeps a voice");
		if (voices > 1) Check(selected[1].id == 100, "the nearest ordinary enemy follows the elite");
		std::set<size_t> slots;
		for (const auto& cue : selected) slots.insert(cue.slot);
		Check(slots.size() == selected.size(), "every voice has its own cadence slot");
	}
	Settings oversize; oversize.maxVoices = 50;
	Check(Select(world, pack, oversize).size() == MaxVoices, "the voice count is bounded by the slot array");
	Settings none; none.maxVoices = 0;
	Check(Select(world, pack, none).size() == 1, "a zero voice count still allows one voice");
}
// The "!" over an NPC rings once when it appears, again only after it went away.
void NpcAlerts() {
	const auto world = World();
	auto akara = Make(1, Kind::Npc, 40, 30), warriv = Make(2, Kind::Npc, 33, 30), boss = Make(3, Kind::Important, 31, 30);
	akara.npcAlert = warriv.npcAlert = boss.npcAlert = true;
	AlertTracker alerts;
	const auto first = alerts.Raised(world, {akara, warriv, boss});
	Check(first.size() == 2 && first[0].id == 2 && first[1].id == 1, "both marks ring, nearest first, and never an enemy's");
	Check(first[0].right > 0, "the bell is placed toward the NPC");
	Check(alerts.Raised(world, {akara, warriv}).empty(), "a mark still shown does not ring again");
	akara.npcAlert = false;
	Check(alerts.Raised(world, {akara, warriv}).empty(), "a mark that went away is silent");
	akara.npcAlert = true;
	const auto again = alerts.Raised(world, {akara, warriv});
	Check(again.size() == 1 && again[0].id == 1, "a mark that comes back rings again");
	Check(alerts.Raised(world, {warriv}).empty() && alerts.Raised(world, {akara, warriv}).empty(),
		"an NPC missing from one read and back with its mark does not ring again");
	alerts.Reset();
	Check(alerts.Raised(world, {akara}).size() == 1, "a new game rings for the marks already shown");
}

void Corpses() {
	CorpseTracker corpses;
	const Radar::Position player{30, 30};
	constexpr double Range = 20;
	auto result = corpses.Update(player, {}, Range);
	Check(result.count == 0 && !result.cue, "no corpse, no cue");
	result = corpses.Update(player, {{40, 30}, {33, 30}, {90, 90}}, Range);
	Check(result.count == 2 && result.cue && std::abs(result.cue->distance - 3) < 1e-9,
		"corpses coming into range cue once, toward the nearest, and the far one is not counted");
	Check(!corpses.Update(player, {{40, 30}, {33, 30}}, Range).cue, "and not again while they stay");
	result = corpses.Update(player, {{30 + Range * 1.1, 30}}, Range);
	Check(result.count == 0 && !result.cue, "one just past the range is not counted");
	Check(!corpses.Update(player, {{30 + Range * 0.9, 30}}, Range).cue, "nor announced again on coming back across the edge");
	(void)corpses.Update(player, {}, Range);
	Check(corpses.Update(player, {{35, 30}}, Range).cue.has_value(), "once all were gone, the next ones cue again");
	corpses.Reset();
	Check(corpses.Update(player, {{35, 30}}, Range).cue.has_value(), "a reset forgets they were available");
	CorpseTracker loop;
	constexpr uint64_t Repeat = 3000;
	Check(loop.Update(player, {{35, 30}}, Range, 1000, Repeat).cue.has_value(), "with a repeat the first cue comes as before");
	Check(!loop.Update(player, {{35, 30}}, Range, 2000, Repeat).cue, "and nothing until the repeat is due");
	Check(loop.Update(player, {{35, 30}}, Range, 4000, Repeat).cue.has_value(), "then it rattles again while they stay");
	Check(!loop.Update(player, {}, Range, 8000, Repeat).cue, "but never with none within range");
}

int main() {
	ScreenArea(); BigPack(); NpcAlerts();
	FilteringAndPriority(); VisibilityAndPlacement(); CadenceAndLifecycle(); FairScheduling(); PackCadence(); FastRepeatsAtRuntimeCadence();
	GrowlsOff(); Corpses();
	Passed();
}
