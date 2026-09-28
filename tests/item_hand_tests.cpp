#include "item_hand.h"
#include "test_support.h"

using namespace D2RAccess::ItemHand;

int main() {
	constexpr uint32_t Gem = 40, Sword = 7, Tome = 9;
	const Under sword{Sword, 2, 0};
	Check(Resolve(Gem, 0, sword, {Sword, 2, 1}, true, false) == Outcome::Socketed,
		"a gem that leaves the hand onto a socketed sword that stays put was socketed");
	// Monastery, 2026-09-28: the SDK read 0 filled sockets on a sword whose one socket held a topaz.
	Check(Resolve(Gem, 0, sword, sword, true, false) == Outcome::Socketed,
		"a socketing is recognised without the filled-socket count changing");
	Check(Resolve(Gem, 0, {Sword}, {Sword}, true, false) == Outcome::Placed,
		"an item with no sockets never claims to have taken a gem");
	Check(Resolve(Gem, 0, {Tome, 0, 0, 5}, {Tome, 0, 0, 6}, false, false) == Outcome::Merged,
		"a scroll that leaves the hand onto a tome that grows went into it");
	// Monastery, 2026-09-28: a gem thrown with X over a sword with no sockets was announced as added to it.
	Check(Resolve(Gem, 0, sword, sword, true, true) == Outcome::Thrown,
		"a gem that leaves the hand by the drop button was thrown away, whatever lies under it");
	Check(Resolve(Gem, 0, {Sword}, {Sword}, false, false) == Outcome::Placed,
		"an unchanged item under the hand never claims to have taken the carried one");
	Check(Resolve(Gem, Sword, sword, {Gem}, true, false) == Outcome::Swapped,
		"the game handing back the sword instead is a swap, not a socketing");
	Check(Resolve(Gem, 0, {}, {Gem}, true, false) == Outcome::Placed, "a gem put into an empty cell was placed");
	Check(Resolve(Gem, 0, sword, {}, true, false) == Outcome::Placed, "a focus that no longer rests on the sword cannot claim a socketing");
	Check(Resolve(Gem, 0, {Gem, 1}, {Gem, 1}, true, false) == Outcome::Placed,
		"a cell still reporting the item just picked up is not a target it can go into");
	Check(Resolve(Gem, Gem, sword, sword, true, true) == Outcome::None, "the same item still in hand is no change");
	Check(Resolve(0, Gem, {}, sword, true, false) == Outcome::None, "a pickup is the caller's own notice");

	Names names;
	names.Remember(Sword, "Mandoble", 6, "Filo de la tormenta. raro");
	Check(names.Recall(Sword, "Mandoble", 6) == "Filo de la tormenta. raro", "a tooltip name is recalled for the same item");
	Check(names.Recall(Sword, "Hacha", 6).empty(), "a recycled id on a different base is not misnamed");
	Check(names.Recall(Sword, "Mandoble", 4).empty(), "a recycled id of another quality is not misnamed");
	names.Remember(0, "Mandoble", 6, "x");
	Check(names.Recall(0, "Mandoble", 6).empty(), "an unknown id is never remembered");
	names.Clear();
	Check(names.Recall(Sword, "Mandoble", 6).empty(), "clearing forgets every name");
	Passed();
}
