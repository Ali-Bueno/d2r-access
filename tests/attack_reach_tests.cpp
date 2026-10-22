#include "attack_reach.h"
#include "test_support.h"

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
	Check(Reach(Style::Melee, {}, settings) == settings.meleeReach, "the naming reach starts at contact");
	Check(Reach(Style::Melee, {.rangeAdder = 2}, settings) == settings.meleeReach + 2,
		"a polearm's rangeadder lengthens the naming reach");
	Check(Reach(Style::Melee, {.rangeAdder = -4}, settings) == settings.meleeReach, "a malformed rangeadder never shortens it");
	Check(Reach(Style::Ranged, {.rangeAdder = 2}, settings) == settings.rangedReach, "ranged reach ignores the melee weapon");
}

// The worked cases of docs/controller-targeting.md#melee-reach.
void GameRule() {
	const auto strikes = [](int dx, int dy, int meleeRange) {
		return InMeleeRange(UnitDistance(dx, dy, PlayerSize, MonsterSize), meleeRange);
	};
	Check(strikes(3, 0, 0) && strikes(3, 1, 0) && strikes(2, 2, 0), "bare-handed, a size-2 monster is struck three subtiles straight or two diagonally");
	Check(!strikes(3, 3, 0) && !strikes(4, 0, 0), "but not three diagonally or four straight");
	Check(strikes(-3, 0, 0) && strikes(0, -3, 0) && !strikes(0, -4, 0), "the rule is the same on every side");
	Check(strikes(4, 0, 2) && !strikes(5, 0, 2), "each rangeadder point is about half a subtile");
	Check(UnitDistance(0, 0, PlayerSize, MonsterSize) == 0 && UnitDistance(1, 1, PlayerSize, MonsterSize) == 0,
		"overlapping footprints are at no distance");
	Check(UnitDistance(4, 0, PlayerSize, 3) == UnitDistance(4, 0, PlayerSize, MonsterSize) - 1,
		"a size-3 monster stands half a subtile nearer");
	Check(UnitDistance(4, 0, PlayerSize, 1) == UnitDistance(4, 0, PlayerSize, MonsterSize) + 1,
		"a size-1 monster stands half a subtile further");
	Check(UnitDistance(10, 3, PlayerSize, MonsterSize) == 2 * 8 + 1, "far apart, the footprints' gap counts twice along its longer side");
	Check(UnitDistance(5, 0, PlayerSize, 4) == 2 * 2, "a size-4 monster leaves the table");
	Check(MeleeRange({}, std::nullopt) == 0 && MeleeRange({.rangeAdder = 3}, 1u) == 3, "the weapon's rangeadder is the melee range");
	Check(MeleeRange({.rangeAdder = 1}, WarlockClass) == 1 + WarlockMeleeBonus, "a Warlock reaches two further");
	Check(MeleeRange({.rangeAdder = -2}, std::nullopt) == 0, "a malformed rangeadder never shortens it");
}

void Nearest() {
	auto world = World();
	const Contact bare;
	Check(!AttackReach::Nearest(world, {Enemy(1, 34.5, 30)}, bare), "an enemy beyond melee range is not in contact");
	const auto near = AttackReach::Nearest(world, {Enemy(1, 33.5, 30), Enemy(2, 32.2, 30)}, bare);
	Check(near && near->id == 2 && near->distance == 0, "the nearest enemy in contact is reported");
	Check(AttackReach::Nearest(world, {Enemy(1, 34.5, 30)}, Contact{2}).has_value(), "a polearm reaches the next subtile");
	EntityRadar::Entity npc = Enemy(3, 31, 30); npc.kind = EntityRadar::Kind::Npc;
	Check(!AttackReach::Nearest(world, {npc}, bare), "a friendly NPC is never a target");
	for (int x = 0; x < Size; ++x) world.rooms[0].cells[32 * Size + x] = 1;
	Check(!AttackReach::Nearest(world, {Enemy(4, 30, 33)}, bare), "an enemy behind a wall is not in contact");
}

void Tracking() {
	const auto world = World();
	const Contact contact; // bare hands: the game strikes at distance 1, the ping lets go beyond 3
	Tracker tracker;
	Check(!tracker.Update(world, {Enemy(1, 36, 30)}, contact), "an approaching enemy is silent beyond reach");
	const auto entered = tracker.Update(world, {Enemy(1, 33.5, 30)}, contact);
	Check(entered && entered->id == 1 && entered->bearing.right > 0, "entering reach pings once, placed toward the enemy");
	Check(!tracker.Update(world, {Enemy(1, 32, 30)}, contact), "staying in reach does not ping again");
	Check(!tracker.Update(world, {Enemy(1, 32, 30), Enemy(2, 31, 30)}, contact), "a second enemy joining does not ping");
	Check(!tracker.Update(world, {Enemy(1, 34.5, 30)}, contact), "stepping just past the edge keeps the spell");
	Check(!tracker.Update(world, {Enemy(1, 33.5, 30)}, contact), "so returning across the edge does not ping");
	Check(!tracker.Update(world, {Enemy(1, 35.5, 30)}, contact), "backing off past the release ends the spell silently");
	Check(tracker.Update(world, {Enemy(1, 32, 30)}, contact).has_value(), "a fresh approach pings again");
	Check(!tracker.Update(world, {}, contact), "killing the last enemy ends the spell");
	Check(tracker.Update(world, {Enemy(3, 31, 30)}, contact).has_value(), "the next enemy to arrive pings");
	tracker.Reset();
	Check(tracker.Update(world, {Enemy(3, 31, 30)}, contact).has_value(), "a reset, as after a menu, pings for an enemy still in reach");
}
}

int main() {
	Styles(); Reaches(); GameRule(); Nearest(); Tracking();
	Passed();
}
