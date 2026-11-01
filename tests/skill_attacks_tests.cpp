#include "generated/skill_attacks.h"
#include "battle/skill_cast.h"
#include "test_support.h"

using namespace D2RAccess;
using Generated::SkillAttack;

// Kinds from the game's own skills.txt, named so a failure says which skill
// changed kind rather than which id.
namespace {
struct Skill { const char* name; int id; SkillAttack kind; };

constexpr Skill Skills[] = {
	{"Attack",             0, SkillAttack::Weapon}, // a sword or a bow, by what is in hand
	{"Throw",              2, SkillAttack::Ranged},
	{"Magic Arrow",        6, SkillAttack::Ranged},
	{"Multiple Shot",     12, SkillAttack::Ranged},
	{"Fire Bolt",         36, SkillAttack::Ranged},
	{"Frozen Orb",        64, SkillAttack::Ranged},
	{"Bone Spear",        84, SkillAttack::Ranged},
	{"Holy Bolt",        101, SkillAttack::Ranged}, // heals allies too, and still strikes monsters
	{"Blessed Hammer",   112, SkillAttack::Ranged},
	{"Jab",               10, SkillAttack::Melee},
	{"Zeal",             106, SkillAttack::Melee},
	{"Bash",             126, SkillAttack::Melee},
	{"Charge",           107, SkillAttack::Melee}, // carries the character to the monster itself
	{"Whirlwind",        151, SkillAttack::Melee},
	{"Kick",               1, SkillAttack::None},  // not the player's to select
	{"Unsummon",           3, SkillAttack::None},
	{"Teleport",          54, SkillAttack::None},
	{"Raise Skeleton",    70, SkillAttack::None},
	{"Corpse Explosion",  74, SkillAttack::None},  // its target is a corpse
	{"Might",             98, SkillAttack::None},  // an aura
	{"Find Potion",      131, SkillAttack::None},  // hand to hand in the table, and not an attack
	{"Leap",             132, SkillAttack::None},
	{"Critical Strike",    9, SkillAttack::None},  // passive
};
}

int main() {
	for (const auto& skill : Skills) Check(Generated::SkillAttackFor(skill.id) == skill.kind, skill.name);
	for (const int skill : {40, 57, 60, 149, 223, 228, 249, 277, 385}) {
		Check(Generated::SkillAttackFor(skill) == SkillAttack::None, "self buff is one-shot even when it also deals damage");
		Check(!Generated::SkillRequiresEnemy(skill), "self buff never requires an enemy");
		Check(SkillCast::MissingEnemy(skill, false) == SkillCast::WithoutEnemy::CastHere, "buff attempts native cast without enemies, terrain or a town restriction");
	}
	for (const int skill : {0, 10, 106, 121, 126, 232, 392}) {
		Check(Generated::SkillRequiresEnemy(skill), "unit-only attack needs an enemy");
		Check(SkillCast::MissingEnemy(skill, false) == SkillCast::WithoutEnemy::Report, "ordinary melee retains no-enemy readout");
		Check(SkillCast::MissingEnemy(skill, true) == SkillCast::WithoutEnemy::RepeatHere, "Shift permits a held strike in place without a target");
	}
	for (const int skill : {100000, 36, 47, 48, 92, 107, 143, 151})
		Check(SkillCast::MissingEnemy(skill, false) == SkillCast::WithoutEnemy::CastHere, "optional/unknown skill attempts cast without inventing an enemy requirement");
	Check(SkillCast::MissingEnemy(-1, false) == SkillCast::WithoutEnemy::Report, "an unreadable skill is fought as the plain Attack");
	Check(Generated::SkillRequiresEnemy(378), "non-attack Blood Oath retains its independent enemy requirement");
	Check(!Generated::SkillRequiresEnemy(3), "Unsummon targets a pet, not an enemy");
	Check(SkillCast::OnUnit(true, false, 0) == 6, "normal A retains native left-unit command");
	Check(SkillCast::OnUnit(false, false, 0) == 13, "normal W retains native right-unit command");
	Check(SkillCast::OnUnit(true, true, 0) == 7, "Shift+A uses native stationary left-unit command");
	Check(SkillCast::OnUnit(false, true, 0) == 14, "native right stationary command is distinct");
	for (const int skill : {107, 151}) {
		Check(SkillCast::OnUnit(true, true, skill) == 10, "stationary movement skill uses native extended left command");
		Check(SkillCast::OnUnit(false, true, skill) == 17, "stationary movement skill uses native extended right command");
	}
	Check(Generated::SkillAttackFor(-1) == SkillAttack::None, "no skill is not an attack");
	Check(Generated::SkillAttackFor(100000) == SkillAttack::None, "a skill the table does not know is not an attack");
	// 'sbw ' Short Bow, 'lxb ' Light Crossbow; 'ssd ' Short Sword, 'jav ' Javelin.
	Check(Generated::ShootingWeapon(0x20776273u), "a short bow shoots");
	Check(Generated::ShootingWeapon(0x2062786Cu), "a light crossbow shoots");
	Check(Generated::ShootingWeapon(0x00776273u), "a NUL-padded code finds the same bow");
	Check(!Generated::ShootingWeapon(0x20647373u), "a short sword does not shoot");
	Check(!Generated::ShootingWeapon(0x2076616Au), "a javelin in hand is swung by the plain Attack");
	Check(!Generated::ShootingWeapon(0), "no weapon does not shoot");
	Passed();
}
