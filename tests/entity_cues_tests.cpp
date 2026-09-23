#include "entity_cues.h"
#include "test_support.h"

#include <cmath>
#include <limits>
#include <map>

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
	Check(Select(world, {melee, object}, {}).empty(), "ordinary enemies and objects have no cue even with hostile-looking flags");
	const std::vector entities{Make(3, Kind::Npc, 32, 30), Make(4, Kind::Npc, 34, 30),
		Make(5, Kind::Important, 38, 30), Make(6, Kind::Resurrector, 36, 30), Make(7, Kind::Ranged, 31, 30)};
	const auto selected = Select(world, entities, {});
	Check(selected.size() == 3 && selected[0].id == 5 && selected[1].id == 6 && selected[2].id == 3,
		"boss and shaman take the two enemy slots, with one nearest NPC independently");
	Check(Select(world, {entities[0], entities[1]}, {}).size() == 1, "a town never creates one stream per NPC");
	Check(Select(world, {Make(8, Kind::Ranged, 32, 30), Make(9, Kind::Ranged, 34, 30)}, {}).size() == 1,
		"ranged packs produce only their nearest representative");
	Settings settings; settings.ranged = false;
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
	const auto edge = Select(world, {Make(2, Kind::Npc, 45, 30)}, {});
	Check(edge.size() == 1 && edge[0].intervalMs == Settings{}.farIntervalMs, "range boundary has the slowest pulse");
	Check(Select(world, {Make(2, Kind::Npc, 45.01, 30)}, {}).empty(), "outside proximity range stays silent");
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
	Check(!tracker.Update(world, {far}, 1000, {}), "distant pulse has a long rest");
	Check(Is(tracker.Update(world, {near}, 1000, {}), 1), "approaching recalculates cadence without waiting for old far deadline");
	Check(!tracker.Update(world, {near}, 1549, {}), "near pulse respects the minimum interval");
	Check(Is(tracker.Update(world, {near}, 1550, {}), 1), "near pulse repeats at the shorter interval");
	Check(!tracker.Update(world, {Make(2, Kind::Npc, 30, 30)}, 1750, {}), "nearest target changes do not burst pulses");
	Check(!tracker.Update(world, {}, 2100, {}), "vanished or dead targets cannot repeat from memory");
	Check(Is(tracker.Update(world, {near}, 2100, {}), 1), "a new visible target starts once after disappearance");
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
	Check(counts.size() == 3 && counts[1] >= 9 && counts[2] >= 9 && counts[3] >= 9,
		"repeated boss cues never starve the shaman or NPC");
	tracker.Reset();
	Check(Is(tracker.Update(world, entities, 0, {}), 1), "first cue prefers important enemies");
	Check(!tracker.Update(world, entities, 159, {}), "simultaneously discovered targets are staggered");
	Check(Is(tracker.Update(world, entities, 160, {}), 2), "shaman follows the important enemy");
}
}
int main() {
	FilteringAndPriority(); VisibilityAndPlacement(); CadenceAndLifecycle(); FairScheduling();
	Passed();
}
