#include "npc_spawn_rules.h"
#include "test_support.h"

#include <map>

using namespace D2RAccess;

namespace {
// InitFns and quest record rows as the game's init functions test them (docs/entity-scan.md).
constexpr int Placeholder1 = 18, Placeholder2 = 19, HratliStart = 49, HratliEnd = 50, Natalya = 52, Drehya = 66, Larzuk = 70;
constexpr int ActTwoIntro = 8, Radament = 9, SevenTombs = 14, ActThreeIntro = 16, Guardian = 22, PrisonOfIce = 37;
constexpr uint16_t Goal = 0x2000, Completed = 0x0001, Step = 0x0004, RewardPending = 0x0002;

NpcSpawnRules::QuestFlags Flags(std::map<int, uint16_t> words) {
	return [words](int row) -> std::optional<uint16_t> {
		const auto found = words.find(row);
		return found == words.end() ? uint16_t{0} : found->second;
	};
}

void Jerhyn() {
	const auto fresh = Flags({});
	Check(NpcSpawnRules::Spawns(Placeholder1, fresh) == true && NpcSpawnRules::Spawns(Placeholder2, fresh) == false,
		"a new character meets Jerhyn at his first place");
	const auto met = Flags({{ActTwoIntro, Goal}});
	Check(NpcSpawnRules::Spawns(Placeholder1, met) == false && NpcSpawnRules::Spawns(Placeholder2, met) == true,
		"once Act II's intro is done he stands at the second");
	Check(NpcSpawnRules::Spawns(Placeholder2, Flags({{Radament, Completed}})) == true, "a quest done in an earlier game counts");
	Check(NpcSpawnRules::Spawns(Placeholder2, Flags({{SevenTombs, Step}})) == true, "so does the Seven Tombs once begun");
	Check(!NpcSpawnRules::Spawns(Placeholder1, [](int) { return std::optional<uint16_t>{}; }), "unreadable flags tell nothing");
}

void Others() {
	Check(NpcSpawnRules::Spawns(HratliStart, Flags({})) == true && NpcSpawnRules::Spawns(HratliEnd, Flags({})) == false,
		"Hratli waits at the docks until Act III's intro is done");
	const auto docked = Flags({{ActThreeIntro, Goal | Completed}});
	Check(NpcSpawnRules::Spawns(HratliStart, docked) == false && NpcSpawnRules::Spawns(HratliEnd, docked) == true, "then at his forge");
	Check(NpcSpawnRules::Spawns(Natalya, Flags({})) == true, "Natalya is in Kurast Docks");
	Check(NpcSpawnRules::Spawns(Natalya, Flags({{Guardian, Goal}})) == false, "until Mephisto falls");
	Check(NpcSpawnRules::Spawns(Drehya, Flags({{PrisonOfIce, Goal | RewardPending}})) == false, "Anya thawed is not in town yet");
	Check(NpcSpawnRules::Spawns(Drehya, Flags({{PrisonOfIce, Goal | Completed}})) == true, "she is once her rescue is rewarded");
	Check(!NpcSpawnRules::Spawns(Larzuk, Flags({})), "a marker whose test the flags cannot tell gives no answer");
}
}

int main() {
	Jerhyn();
	Others();
	Passed();
	return 0;
}
