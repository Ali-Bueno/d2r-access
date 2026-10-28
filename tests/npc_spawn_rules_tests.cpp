#include "npc_spawn_rules.h"
#include "test_support.h"

#include <map>

using namespace D2RAccess;

namespace {
// InitFns and quest record rows as the game's init functions test them (docs/entity-scan.md).
constexpr int Placeholder1 = 18, Placeholder2 = 19, HratliStart = 49, HratliEnd = 50, Natalya = 52, Drehya = 66, NihlathakInTown = 68, Larzuk = 70;
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

// Nihlathak (InitFn 68): in Harrogath until a player of the game goes towards Anya or into his
// temple, and gone for good once her rescue is rewarded (a tester, 2026-10-27).
void Nihlathak() {
	NpcSpawnRules::GameLevels levels;
	constexpr uint64_t Game = 7, NextGame = 8;
	const NpcSpawnRules::ThisGame game{[&levels](int level) { return levels.Entered(Game, level); }};
	levels.Note(Game, 109);
	levels.Note(Game, 111);
	Check(NpcSpawnRules::Spawns(NihlathakInTown, Flags({}), game) == true, "he is in town at the start of Act V");
	Check(NpcSpawnRules::Spawns(NihlathakInTown, Flags({{PrisonOfIce, Step}}), game) == true, "Anya's quest given, he still is");
	levels.Note(Game, 112);
	Check(NpcSpawnRules::Spawns(NihlathakInTown, Flags({}), game) == false, "not once the Arreat Plateau has been entered");
	levels.Note(NextGame, 109);
	Check(levels.Entered(NextGame, 109) && !levels.Entered(NextGame, 112) && !levels.Entered(Game, 109), "each game keeps its own levels");
	const NpcSpawnRules::ThisGame next{[&levels](int level) { return levels.Entered(NextGame, level); }};
	Check(NpcSpawnRules::Spawns(NihlathakInTown, Flags({}), next) == true, "a new game has him back");
	Check(NpcSpawnRules::Spawns(NihlathakInTown, Flags({{PrisonOfIce, Goal | Completed}}), next) == false,
		"never once Anya's rescue is rewarded");
	Check(NpcSpawnRules::Spawns(NihlathakInTown, Flags({{PrisonOfIce, Goal | RewardPending}}), next) == true,
		"the game tests the reward's bits alone");
	Check(!NpcSpawnRules::Spawns(NihlathakInTown, Flags({}), {next.entered, true}), "with other players in the game it cannot be told");
	Check(!NpcSpawnRules::Spawns(NihlathakInTown, Flags({})), "nor without the game's levels");
}
}

int main() {
	Jerhyn();
	Others();
	Nihlathak();
	Passed();
	return 0;
}
