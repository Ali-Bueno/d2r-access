#include "crosshair.h"
#include "test_support.h"

using namespace D2RAccess;
using namespace Crosshair;
namespace {
constexpr int Size = 60;
constexpr uint32_t Monster = 1, Object = 2;
Radar::World World() {
	Radar::World world;
	world.player = {30, 30}; world.walkingMask = 1;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size, 0)});
	return world;
}
EntityRadar::Entity Unit(uint32_t id, double x, double y, EntityRadar::Kind kind = EntityRadar::Kind::Enemy) {
	EntityRadar::Entity entity;
	entity.id = id; entity.kind = kind; entity.position = {x, y};
	return entity;
}

void Locks() {
	const auto world = World();
	const std::vector entities{Unit(1, 36, 30), Unit(2, 30, 24), Unit(3, 33, 30, EntityRadar::Kind::Npc)};
	Tracker tracker;
	Check(!tracker.Update(world, entities, std::nullopt, 0), "no target, no beep");
	const auto first = tracker.Update(world, entities, Aim{Monster, 1}, 0);
	Check(first && first->id == 1 && first->bearing.right > 0, "the game targeting an enemy beeps, placed toward it");
	Check(!tracker.Update(world, entities, Aim{Monster, 1}, 1000), "holding the same target stays quiet");
	Check(!tracker.Update(world, entities, Aim{Monster, 3}, 2000), "a friendly NPC targeted is not an enemy in the sights");
	Check(!tracker.Update(world, entities, Aim{Object, 2}, 3000), "an object sharing an enemy's id is not that enemy");
	Check(!tracker.Update(world, entities, Aim{Monster, 9}, 4000), "a target no read unit carries cannot be placed");
	const auto back = tracker.Update(world, entities, Aim{Monster, 1}, 5000);
	Check(back && back->id == 1, "finding the target again after the repeat time beeps again");
	const auto hop = tracker.Update(world, entities, Aim{Monster, 2}, 6000);
	Check(hop && hop->id == 2 && hop->bearing.front > 0, "a new enemy in the sights beeps");
}

// Live 2026-10-22: the target traded 340, 324, 340, 326, 340 in three seconds, a beep each.
void Trading() {
	const auto world = World();
	const std::vector entities{Unit(1, 36, 30), Unit(2, 30, 24), Unit(3, 24, 30)};
	Tracker tracker;
	const Settings settings;
	Check(tracker.Update(world, entities, Aim{Monster, 1}, 0, settings).has_value(), "the first enemy beeps");
	Check(tracker.Update(world, entities, Aim{Monster, 2}, 300, settings).has_value(), "a second enemy beeps");
	Check(!tracker.Update(world, entities, Aim{Monster, 1}, 600, settings), "the target back on the first stays quiet");
	Check(tracker.Update(world, entities, Aim{Monster, 3}, 900, settings).has_value(), "a third enemy still beeps");
	Check(!tracker.Update(world, entities, std::nullopt, 1200, settings), "the target lost");
	Check(!tracker.Update(world, entities, Aim{Monster, 2}, 1500, settings), "back within the repeat time stays quiet");
	uint64_t now = 1500;
	for (; now < 1500 + settings.repeatMs; now += 100)
		Check(!tracker.Update(world, entities, Aim{Monster, 2}, now, settings), "a target held stays quiet");
	const auto again = tracker.Update(world, entities, Aim{Monster, 3}, now, settings);
	Check(again && again->id == 3, "an enemy not the target for the repeat time beeps again");
	Check(!tracker.Update(world, entities, Aim{Monster, 2}, now + settings.gapMs, settings),
		"the one held all along still counts as heard");
}

void Hops() {
	const auto world = World();
	const std::vector entities{Unit(1, 36, 30), Unit(2, 30, 24)};
	Tracker tracker;
	const Settings settings;
	Check(tracker.Update(world, entities, Aim{Monster, 1}, 0, settings).has_value(), "the first lock beeps");
	Check(!tracker.Update(world, entities, Aim{Monster, 2}, settings.gapMs - 1, settings),
		"a hop sooner than the gap waits");
	const auto later = tracker.Update(world, entities, Aim{Monster, 2}, settings.gapMs, settings);
	Check(later && later->id == 2, "the target that held past the gap beeps on the next poll");
	tracker.Reset();
	Check(tracker.Update(world, entities, Aim{Monster, 2}, settings.gapMs, settings).has_value(),
		"a reset, as after a menu, beeps for the target still held");
}

void Walls() {
	auto world = World();
	for (int x = 0; x < Size; ++x) world.rooms[0].cells[32 * Size + x] = 1;
	Tracker tracker;
	Check(!tracker.Update(world, {Unit(1, 30, 35)}, Aim{Monster, 1}, 0),
		"the game aims through walls, the beep does not");
}
}

int main() {
	Locks(); Hops(); Trading(); Walls();
	Passed();
}
