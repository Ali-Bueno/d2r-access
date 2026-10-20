#pragma once

#include "generated/skill_attacks.h"
#include <cstdint>

namespace D2RAccess::SkillCast {
// These are native SendSkill (0x102B70) command choices. The stationary
// variants carry Shift's 0x20 flag into the unit command rather than asking
// the native actor to approach the target. Charge/Whirlwind use its extended
// command family; their intrinsic skill movement remains the game's decision.
constexpr uint32_t OnUnit(bool left, bool stationary, int skill) {
	if (!stationary) return left ? 0x06u : 0x0Du;
	const bool extended = skill == 107 || skill == 151;
	return left ? (extended ? 0x0Au : 0x07u) : (extended ? 0x11u : 0x0Eu);
}

enum class WithoutEnemy { Report, CastHere, RepeatHere };
// Non-attacks cast once. Optional-target attacks can also fire with no enemy;
// a held stationary attack repeats in place. No terrain/enemy scan is needed
// to determine either fact, and no policy suppresses a cast because of town.
constexpr WithoutEnemy MissingEnemy(int skill, bool stationary) {
	if (stationary) return WithoutEnemy::RepeatHere;
	return Generated::SkillRequiresEnemy(skill) ? WithoutEnemy::Report : WithoutEnemy::CastHere;
}
}
