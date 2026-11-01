#pragma once

#include "generated/skill_attacks.h"
#include <cstdint>

// Which native cast command a skill uses, and what a cast does without an
// enemy. Ported from bladestorm360's 97cb7c4; the command numbers are the
// game's SendSkill (RVA 0x102B70) client-to-server messages, as the
// stationary variants carry Shift's 0x20 flag (docs/battle/skill-casting.md).
namespace D2RAccess::SkillCast {
constexpr uint32_t LeftOnUnit = 0x06, RightOnUnit = 0x0D;
constexpr uint32_t LeftOnUnitStill = 0x07, RightOnUnitStill = 0x0E;
// Charge and Whirlwind take the extended family of commands.
constexpr uint32_t LeftOnUnitStillExtended = 0x0A, RightOnUnitStillExtended = 0x11;
// skills.txt *Id: Charge (107) and Whirlwind (151).
constexpr int SkillCharge = 107, SkillWhirlwind = 151;

constexpr uint32_t OnUnit(bool left, bool stationary, int skill) {
	if (!stationary) return left ? LeftOnUnit : RightOnUnit;
	const bool extended = skill == SkillCharge || skill == SkillWhirlwind;
	return left ? (extended ? LeftOnUnitStillExtended : LeftOnUnitStill)
		: (extended ? RightOnUnitStillExtended : RightOnUnitStill);
}

enum class WithoutEnemy { Report, CastHere, RepeatHere };
// A skill that needs no enemy casts once where the player stands; one that
// needs an enemy says so. A held stationary attack repeats in place.
constexpr WithoutEnemy MissingEnemy(int skill, bool stationary) {
	if (stationary) return WithoutEnemy::RepeatHere;
	return Generated::SkillRequiresEnemy(skill) ? WithoutEnemy::Report : WithoutEnemy::CastHere;
}
}
