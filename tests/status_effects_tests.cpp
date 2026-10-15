#include "status_effects.h"
#include "test_support.h"

#include <algorithm>
#include <set>

using namespace D2RAccess::StatusEffects;

namespace {
size_t IndexOf(int state) {
	for (size_t i = 0; i < Effects.size(); ++i) if (Effects[i].state == state) return i;
	return Effects.size();
}
}

int main() {
	// The table: each state once, each named exactly one way.
	std::set<int> states;
	for (const auto& effect : Effects) {
		states.insert(effect.state);
		const int ways = (effect.condition >= 0) + (effect.skill >= 0) + (effect.curseName >= 0);
		Check(ways == 1, "an effect is named by a condition, a skill or a curse name");
		Check(effect.condition < static_cast<int>(Condition::Count) && effect.curseName < static_cast<int>(CurseName::Count),
			"its name is within its list");
	}
	Check(states.size() == Effects.size(), "no state is listed twice");
	for (const int buff : {89, 128, 141, 156}) Check(!states.contains(buff), "curse=1 buffs and shrines are not harmful states");
	Check(!Effects[IndexOf(2)].Curse() && Effects[IndexOf(9)].Curse() && Effects[IndexOf(9)].skill == 66,
		"poison is a condition, Amplify Damage a curse named by its skill");

	const size_t poison = IndexOf(2), cold = IndexOf(11), amplify = IndexOf(9);
	Tracker tracker;
	Check(tracker.Update({poison}, 1000).Empty(), "entering a game takes a silent baseline");
	auto change = tracker.Update({poison, amplify}, 1100);
	Check(change.started == std::vector<size_t>{amplify} && change.ended.empty(), "a new curse begins");
	change = tracker.Update({amplify}, 1200);
	Check(change.Empty(), "an end waits");
	Check(tracker.Update({amplify}, 1200 + EndHoldMs - 1).Empty(), "until the hold is over");
	change = tracker.Update({amplify}, 1200 + EndHoldMs);
	Check(change.ended == std::vector<size_t>{poison}, "then it is said");
	Check(tracker.Active() == std::vector<size_t>{amplify}, "the effects on the player now");

	// A chill renewed by the next hit is one chill.
	change = tracker.Update({amplify, cold}, 5000);
	Check(change.started == std::vector<size_t>{cold}, "a chill begins");
	(void)tracker.Update({amplify}, 5100);
	change = tracker.Update({amplify, cold}, 5100 + EndHoldMs - 1);
	Check(change.Empty(), "a chill back within the hold neither ended nor began");
	(void)tracker.Update({amplify}, 7000);
	change = tracker.Update({amplify}, 7000 + EndHoldMs);
	Check(change.ended == std::vector<size_t>{cold}, "a chill gone for good ends once");
	Check(tracker.Update({amplify}, 9000).Empty(), "and only once");

	tracker.Reset();
	Check(tracker.Update({cold}, 10000).Empty() && tracker.Update({}, 10100).Empty(), "a reset takes a new baseline");
	Check(tracker.Update({}, 10100 + EndHoldMs).ended == std::vector<size_t>{cold}, "an effect on at the baseline still ends");
	Passed();
	return 0;
}
