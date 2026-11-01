#include "skill_prerequisites.h"
#include "test_support.h"

#include <algorithm>

using namespace D2RAccess;

int main() {
	// Ids from the generated tree: Might 98, Holy Fire 102, Sanctuary 119 (Thorns 103, Holy Freeze 114).
	Check(SkillPrerequisites::Of(102) == std::vector<int>{98}, "Holy Fire needs Might");
	Check(SkillPrerequisites::Of(119) == std::vector<int>{103, 114}, "two requirements keep the table order");
	Check(SkillPrerequisites::Of(98).empty(), "a starting skill needs nothing");
	Check(SkillPrerequisites::Of(-1).empty() && SkillPrerequisites::Of(65535).empty(), "an id with no tree entry needs nothing");
	for (const auto& skill : Generated::SkillTree)
		for (const auto required : skill.required) {
			if (required == Generated::NoRequiredSkill) continue;
			Check(required != skill.skill && std::any_of(Generated::SkillTree.begin(), Generated::SkillTree.end(),
				[&](const Generated::TreeSkill& other) { return other.skill == required && other.playerClass == skill.playerClass; }),
				"every requirement is another skill of the same class's tree");
		}
	Passed();
}
