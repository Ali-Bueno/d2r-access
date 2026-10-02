#include "attack_reach.h"
#include "test_support.h"

#include <limits>

using namespace D2RAccess;
using namespace AttackReach;
namespace {
constexpr int Size = 60;
Radar::World World() {
	Radar::World world;
	world.player = {30, 30}; world.walkingMask = 1;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size, 0)});
	return world;
}
EntityRadar::Entity Enemy(uint32_t id, double x, double y) {
	EntityRadar::Entity entity;
	entity.id = id; entity.kind = EntityRadar::Kind::Enemy; entity.position = {x, y};
	return entity;
}
Skill Of(SkillRange range, bool damage = false) { return {range, damage}; }

void Styles() {
	Check(Classify({}) == Style::Melee, "nothing bound and no bow is a melee fighter");
	Check(Classify({.missileWeapon = true}) == Style::Ranged, "a bow or crossbow in hand shoots");
	Check(Classify({{Of(SkillRange::Both)}, true}) == Style::Ranged, "Attack with a bow shoots");
	Check(Classify({{Of(SkillRange::Both)}}) == Style::Melee, "Attack with a sword swings");
	Check(Classify({{Of(SkillRange::HandToHand), Of(SkillRange::Ranged)}}) == Style::Ranged,
		"a bound ranged skill reaches further than a melee one beside it");
	Check(Classify({{Of(SkillRange::HandToHand), Of(SkillRange::None, true)}}) == Style::Melee,
		"a melee skill outweighs a damaging buff such as Holy Shield");
	Check(Classify({{Of(SkillRange::Both), Of(SkillRange::None, true)}}) == Style::Ranged,
		"a damaging spell such as Fire Bolt makes a caster ranged even with Attack bound");
	Check(Classify({{Of(SkillRange::None), Of(SkillRange::Blank)}}) == Style::Melee,
		"buffs without damage and empty rows give no ranged evidence");
	Check(Resolve(Mode::Melee, {.missileWeapon = true}) == Style::Melee
		&& Resolve(Mode::Ranged, {}) == Style::Ranged && Resolve(Mode::Auto, {.missileWeapon = true}) == Style::Ranged,
		"a fixed mode overrides the loadout");
}

void Reaches() {
	const Settings settings;
	Check(Reach(Style::Melee, {}, settings) == settings.meleeReach, "melee reach starts at contact");
	Check(Reach(Style::Melee, {.rangeAdder = 2}, settings) == settings.meleeReach + 2,
		"a polearm's rangeadder lengthens the melee reach");
	Check(Reach(Style::Melee, {.rangeAdder = -4}, settings) == settings.meleeReach, "a malformed rangeadder never shortens it");
	Check(Reach(Style::Ranged, {.rangeAdder = 2}, settings) == settings.rangedReach, "ranged reach ignores the melee weapon");
	Check(settings.rangedReach == EntityRadar::Settings{}.range, "a shot reaches every enemy the presence cues play");
}

void Nearest() {
	auto world = World();
	Check(!AttackReach::Nearest(world, {Enemy(1, 34, 30)}, 3), "an enemy beyond reach is not in reach");
	const auto near = AttackReach::Nearest(world, {Enemy(1, 34, 30), Enemy(2, 32, 30)}, 3);
	Check(near && near->id == 2 && near->bearing.distance == 2, "the nearest enemy within reach is reported");
	EntityRadar::Entity npc = Enemy(3, 31, 30); npc.kind = EntityRadar::Kind::Npc;
	Check(!AttackReach::Nearest(world, {npc}, 3), "a friendly NPC is never a target");
	for (int x = 0; x < Size; ++x) world.rooms[0].cells[32 * Size + x] = 1;
	Check(!AttackReach::Nearest(world, {Enemy(4, 30, 33)}, 5), "an enemy behind a wall is not within reach");
	Check(!AttackReach::Nearest(World(), {Enemy(5, 31, 30)}, std::numeric_limits<double>::quiet_NaN()),
		"an invalid reach finds nothing");
}

void Tracking() {
	const auto world = World();
	Tracker tracker;
	Check(!tracker.Update(world, {Enemy(1, 36, 30)}, 3, 1), "an approaching enemy is silent beyond reach");
	const auto entered = tracker.Update(world, {Enemy(1, 32.5, 30)}, 3, 1);
	Check(entered && entered->id == 1 && entered->bearing.right > 0, "entering reach pings once, placed toward the enemy");
	Check(!tracker.Update(world, {Enemy(1, 32, 30)}, 3, 1), "staying in reach does not ping again");
	Check(!tracker.Update(world, {Enemy(1, 32, 30), Enemy(2, 31, 30)}, 3, 1), "a second enemy joining does not ping");
	Check(!tracker.Update(world, {Enemy(1, 33.5, 30)}, 3, 1), "stepping just past the edge keeps the spell");
	Check(!tracker.Update(world, {Enemy(1, 32.9, 30)}, 3, 1), "so returning across the edge does not ping");
	Check(!tracker.Update(world, {Enemy(1, 34.5, 30)}, 3, 1), "backing off past the release ends the spell silently");
	Check(tracker.Update(world, {Enemy(1, 32, 30)}, 3, 1).has_value(), "a fresh approach pings again");
	Check(!tracker.Update(world, {}, 3, 1), "killing the last enemy ends the spell");
	Check(tracker.Update(world, {Enemy(3, 31, 30)}, 3, 1).has_value(), "the next enemy to arrive pings");
	tracker.Reset();
	Check(tracker.Update(world, {Enemy(3, 31, 30)}, 3, 1).has_value(), "a reset, as after a menu, pings for an enemy still in reach");
}
}

int main() {
	Styles(); Reaches(); Nearest(); Tracking();
	Passed();
}
