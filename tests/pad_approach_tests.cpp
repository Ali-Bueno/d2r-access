#include "pad_approach.h"
#include "test_support.h"

using namespace D2RAccess;
using PadApproach::Next;
using PadApproach::NothingElseAtHand;
using PadApproach::ByAim;
using PadApproach::NpcTargeting;

namespace {
std::array<int32_t, PadApproach::Slots> Empty() {
	std::array<int32_t, PadApproach::Slots> ids;
	ids.fill(PadApproach::EmptySlot);
	return ids;
}
}

int main() {
	constexpr NpcTargeting Game{4.f, 1.f};
	PadApproach::State state;

	Check(!Next(state, false, Game), "not wanted from the start, nothing is written");
	Check(Next(state, true, Game) == ByAim, "wanted replaces the game's values");
	Check(!Next(state, true, ByAim), "already raised, nothing is written again");
	Check(Next(state, true, Game) == ByAim, "the game loading its own value is raised again");
	Check(Next(state, false, ByAim) == Game, "not wanted gives the game its values back");
	Check(!Next(state, false, Game), "and only once");

	PadApproach::State reloaded;
	(void)Next(reloaded, true, Game);
	Check(!Next(reloaded, false, Game), "the game having reloaded, its values are left alone");

	PadApproach::State same;
	Check(!Next(same, true, ByAim) && !Next(same, false, ByAim), "game values equal to ours are never changed");

	PadApproach::State other;
	(void)Next(other, true, NpcTargeting{3.f, .5f});
	Check(Next(other, false, ByAim) == NpcTargeting{3.f, .5f}, "what is given back is what was found, not fixed values");

	PadApproach::State half;
	(void)Next(half, true, Game);
	Check(!Next(half, false, NpcTargeting{35.f, 1.f}), "only one of ours left: the game has loaded, leave it");

	PadApproach::State flips;
	(void)Next(flips, true, Game);
	(void)Next(flips, false, ByAim);
	Check(Next(flips, true, Game) == ByAim && Next(flips, false, ByAim) == Game, "walking past a stash lowers and raises it again");

	Check(NothingElseAtHand(Empty()), "all slots empty");
	for (const size_t slot : PadApproach::OtherTargets) {
		auto ids = Empty();
		ids[slot] = 7;
		Check(!NothingElseAtHand(ids), "a monster, object, item, exit or corpse at hand keeps the game's reach");
	}
	auto people = Empty();
	people[0] = 7; // another player: in reach across the whole town
	people[5] = 7; // the NPC itself
	Check(NothingElseAtHand(people), "another player or an NPC is no reason to keep it");
	Passed();
}
