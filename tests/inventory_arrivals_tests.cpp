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
	Passed();
}
