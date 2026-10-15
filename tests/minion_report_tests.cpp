#include "minion_report.h"
#include "test_support.h"

#include <map>
#include <string>
#include <vector>

using namespace D2RAccess::MinionReport;

namespace {
// PetType rows of the game's pettype.txt (Rotw bank) the report meets.
constexpr int Golem = 3, Skeleton = 4, SkeletonMage = 5, Revive = 6, Hireling = 7, Raven = 10, SpiritWolf = 11,
	Totem = 13, Demon = 20;
Kind KindOf(int type) {
	switch (type) {
	case Golem: return {IconType::Single, true, "Golem"};
	case Skeleton: return {IconType::Counted, true, "Skeleton"};
	case SkeletonMage: return {IconType::Counted, true, "Mage"};
	case Revive: return {IconType::Counted, true, "Revived"};
	case Raven: return {IconType::Counted, false, "Raven"};
	case SpiritWolf: return {IconType::Counted, true, "Wolf"};
	case Totem: return {IconType::Single, true, "Spirit"};
	case Demon: return {IconType::Each, true, "Demonic Servant"};
	default: return {}; // the hireling and traps have no portrait
	}
}
const std::map<int, std::string> Names{{290, "Clay Golem"}, {363, "Skeleton"}, {364, "Skeletal Mage"},
	{419, "Raven"}, {420, "Spirit Wolf"}, {423, "Oak Sage"}, {742, "Goatman"}, {743, "Tainted"}, {5, "Zombie"}, {19, "Fallen"}};
std::string ClassName(uint16_t id) {
	const auto found = Names.find(id);
	return found == Names.end() ? std::string{} : found->second;
}
const Words English{"%s: %d", "%s: life %d percent", "%s %d"};
std::vector<std::string> Report(const std::vector<Pet>& pets) {
	std::vector<std::string> texts;
	for (const auto& entry : Entries(pets, KindOf, ClassName, English)) texts.push_back(entry.text);
	return texts;
}
}

int main() {
	Check(Report({}).empty(), "no summons, nothing said");
	Check(Report({{338, Hireling, 80}}).empty(), "the hireling has its own line in the report");

	// The tester's druid: "Ravens: 5, Spirit Wolves: 2, Oak Sage: Life 35%".
	std::vector<Pet> druid;
	for (int i = 0; i < 5; ++i) druid.push_back({419, Raven, 100});
	druid.push_back({423, Totem, 35});
	druid.push_back({420, SpiritWolf, 90});
	druid.push_back({420, SpiritWolf, 40});
	Check(Report(druid) == std::vector<std::string>{"Raven: 5", "Spirit Wolf: 2", "Oak Sage: life 35 percent"},
		"counted types by their pets' own name, a single one with its life, in the portraits' order");

	// The necromancer: skeletons, mages, a golem, and revives of two kinds.
	const auto necro = Report({{363, Skeleton, 100}, {363, Skeleton, 50}, {364, SkeletonMage, 100}, {290, Golem, 72},
		{5, Revive, 100}, {19, Revive, 100}});
	Check(necro == std::vector<std::string>{"Clay Golem: life 72 percent", "Skeleton: 2", "Skeletal Mage: 1", "Revived: 2"},
		"revived monsters of different kinds go by their type's name");

	// The warlock's demons, each its own, numbered where a name repeats.
	const auto warlock = Report({{742, Demon, 65}, {743, Demon, 100}, {743, Demon, 75}});
	Check(warlock == std::vector<std::string>{"Goatman: life 65 percent", "Tainted 1: life 100 percent", "Tainted 2: life 75 percent"},
		"each demon with its life, a repeated name numbered");

	Check(Report({{999, Golem, 50}}) == std::vector<std::string>{"Golem: life 50 percent"}, "an unnamed class falls back to its type's name");
	const auto entries = Entries({{420, SpiritWolf, 90}, {742, Demon, 65}}, KindOf, ClassName, English);
	Check(entries.size() == 2 && entries[0].pet == -1 && entries[1].pet == 1, "a single entry names its pet, a count none");
	// BACK+RT, one entry per press.
	Stepper stepper;
	Check(stepper.Next(0, 1000) == -1, "nothing to step through");
	Check(stepper.Next(3, 2000) == 0 && stepper.Next(3, 3000) == 1 && stepper.Next(3, 4000) == 2, "each press the next");
	Check(stepper.Next(3, 5000) == 0, "after the last, the first again");
	Check(stepper.Next(2, 6000) == 1 && stepper.Next(2, 7000) == 0, "the list is made again at each press");
	Check(stepper.Next(3, 7000 + RestartMs) == 0, "a press after a pause starts at the first");
	Passed();
	return 0;
}
