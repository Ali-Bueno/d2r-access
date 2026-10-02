#include "native_attack.h"
#include "equipment_census.h"
#include "native_ground_items.h"
#include "native_skill_bar.h"
#include "weapon_swap.h"

#include <algorithm>

namespace D2RAccess::NativeAttack {
std::optional<AttackReach::Loadout> ReadLoadout(const D2RL::PluginContext* context) {
	AttackReach::Loadout loadout;
	auto skills = NativeSkillBar::BoundSkills();
	if (skills) loadout.skills = std::move(*skills);
	std::vector<Vitals::Breakage::Item> worn;
	const bool equipment = EquipmentCensus::Read(context, worn) == EquipmentCensus::Result::Read;
	for (const auto& item : worn) {
		if (item.bodyLocation != WeaponSwap::RightHand && item.bodyLocation != WeaponSwap::LeftHand) continue;
		const auto weapon = NativeGroundItems::WeaponOf(item.classId);
		if (!weapon) continue;
		loadout.missileWeapon |= weapon->missile;
		if (!weapon->missile) loadout.rangeAdder = std::max(loadout.rangeAdder, weapon->rangeAdder);
	}
	if (!skills && !equipment) return std::nullopt;
	return loadout;
}
}
