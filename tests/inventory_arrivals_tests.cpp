#include "inventory_arrivals.h"
#include "test_support.h"

using namespace D2RAccess::InventoryArrivals;

int main() {
	const Item ruby{10, 1, 2, Place::Backpack, 0, 0}, helm{11, 2, 4, Place::Worn}, gem{12, 3, 2, Place::Backpack, 5, 1};
	// Akara, 2026-09-28: a bought sword landed in column 3 and was never found until the panel was reopened.
	const Item sword{20, 4, 4, Place::Backpack, 2, 0};
	auto found = Diff({ruby, helm}, {ruby, helm, sword}, 0);
	Check(found.size() == 1 && found[0].item.id == 20 && !found[0].unequipped, "a bought item new to the backpack arrives");
	Item potion{21, 5, 2, Place::Belt, 3, 0};
	found = Diff({ruby}, {ruby, potion}, 0);
	Check(found.size() == 1 && found[0].item.place == Place::Belt, "a potion bought straight into the belt arrives there");
	Item takenOff = helm;
	takenOff.place = Place::Backpack;
	found = Diff({ruby, helm}, {ruby, takenOff}, 0);
	Check(found.size() == 1 && found[0].unequipped, "a worn item that moves into the backpack was taken off");
	Check(Diff({ruby}, {ruby, gem}, 12).empty(), "the item placed from the hand is the hand's own notice");
	Item sorted = ruby;
	sorted.x = 7;
	Check(Diff({ruby, gem}, {sorted, gem}, 0).empty(), "items moved within the backpack by sorting are not arrivals");
	Item equipped = ruby;
	equipped.place = Place::Worn;
	Check(Diff({ruby}, {equipped}, 0).empty(), "an item put on is not an arrival in the backpack");
	Item stashed{30, 6, 2, Place::Other};
	Check(Diff({}, {stashed}, 0).empty(), "a new item outside the backpack and belt is not announced");

	// Y with the stash open sorts a gem away (a tester).
	const auto gone = Departures({ruby, gem, helm}, {ruby, helm}, 0);
	Check(gone.size() == 1 && gone[0].id == 12, "a gem that left the backpack departed");
	Check(Departures({ruby, gem}, {ruby}, 12).empty(), "one picked up into the hand did not");
	Check(Departures({ruby, helm}, {ruby}, 0).empty() && Departures({ruby}, {sorted}, 0).empty(),
		"neither does a worn item nor one moved within the backpack");

	// The stashes, read while the stash is open (a tester, 2026-10-14).
	const Item topaz{40, 7, 2, Place::PersonalStash, 3, 3}, amethyst{41, 8, 2, Place::SharedStash, 0, 0};
	const auto fromStash = Departures({ruby, topaz, amethyst}, {ruby, amethyst}, 0);
	Check(fromStash.size() == 1 && fromStash[0].id == 40, "a gem Y sorts out of the personal stash departed");
	Check(Departures({amethyst}, {}, 0).size() == 1, "and one out of the shared stash");
	Item intoStash = gem;
	intoStash.place = Place::SharedStash;
	Check(Departures({ruby, gem}, {ruby, intoStash}, 0).empty(), "an item that went into a stash has not left for a tab");
	const auto sent = Stashed({ruby, gem}, {ruby, intoStash}, 0);
	Check(sent.size() == 1 && sent[0].id == 12 && sent[0].place == Place::SharedStash, "the backpack's item sent to a stash is named there");
	Check(Stashed({ruby, gem}, {ruby, intoStash}, 12).empty(), "one placed from the hand is the hand's own notice");
	Item moved = topaz;
	moved.x = 0;
	Check(Stashed({topaz, helm}, {moved, helm}, 0).empty() && Stashed({}, {topaz}, 0).empty(),
		"neither a stash item moved within the stash nor one first seen there is news");
	Item takenOut = topaz;
	takenOut.place = Place::Backpack;
	found = Diff({ruby, topaz}, {ruby, takenOut}, 0);
	Check(found.size() == 1 && found[0].item.id == 40 && !found[0].unequipped, "an item sent from a stash to the backpack arrives there");
	Passed();
}
