#include "charge_tracker.h"
#include "generated/skill_states.h"
#include "test_support.h"

#include <array>
#include <set>

using namespace D2RAccess::ChargeTracker;

namespace {
using Counts = std::array<int, 2>;
std::vector<Change> Read(Tracker& tracker, Counts counts, bool swinging, uint64_t now) {
	return tracker.Update(counts, swinging, now);
}
bool Is(const std::vector<Change>& changes, size_t index, int count) {
	return changes.size() == 1 && changes[0].index == index && changes[0].count == count;
}
}

int main() {
	// The generated table: the six charge-up skills, each with its own ItemStatCost row.
	std::set<int> stats;
	for (const auto& entry : D2RAccess::Generated::SkillStates) if (entry.charges >= 0) stats.insert(entry.charges);
	Check(stats == std::set<int>{318, 319, 320, 321, 322, 323}, "the six charge stats of ItemStatCost.txt");

	Tracker tracker;
	Check(Read(tracker, {2, 0}, false, 0).empty(), "the first read is the baseline");
	Check(Read(tracker, {2, 0}, false, 100).empty(), "nothing changed, nothing said");
	Check(Is(Read(tracker, {3, 0}, true, 200), 0, 3), "a new charge is said with its count");
	Check(Is(Read(tracker, {3, 1}, true, 300), 1, 1), "another skill's charges are its own");
	Check(Read(tracker, {0, 0}, true, 400).empty(), "charges released by a swing are not said");

	Check(Is(Read(tracker, {1, 0}, true, 1000), 0, 1), "charging again");
	Check(Read(tracker, {0, 0}, false, 1000 + ReleaseWindowMs).empty(), "the blow lands as the swing ends");
	Check(Is(Read(tracker, {1, 0}, true, 2000), 0, 1) && Read(tracker, {1, 0}, false, 2100).empty(), "charged once more");
	Check(Is(Read(tracker, {0, 0}, false, 2001 + ReleaseWindowMs), 0, 0), "charges gone long after a swing ran out");

	Check(Read(tracker, {-1, 0}, false, 5000).empty(), "a negative reading is none");
	tracker.Reset();
	Check(Read(tracker, {3, 3}, false, 6000).empty(), "a reset takes a new baseline");
	Check(Is(Read(tracker, {3, 0}, false, 6100), 1, 0), "never having swung, charges run out");
	Passed();
	return 0;
}
