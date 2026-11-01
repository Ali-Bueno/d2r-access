#pragma once

#include "generated/skill_tree.h"

#include <algorithm>
#include <vector>

namespace D2RAccess::SkillPrerequisites {
// The skill ids a skill needs a point in (skills.txt reqskill1..3); empty for a starting skill.
inline std::vector<int> Of(int skill) {
	std::vector<int> ids;
	const auto entry = std::find_if(Generated::SkillTree.begin(), Generated::SkillTree.end(),
		[skill](const Generated::TreeSkill& tree) { return tree.skill == skill; });
	if (entry == Generated::SkillTree.end()) return ids;
	for (const auto required : entry->required) if (required != Generated::NoRequiredSkill) ids.push_back(required);
	return ids;
}
}
