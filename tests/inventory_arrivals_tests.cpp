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

	// The Horadric Cube (a tester, 2026-10-18).
	const Item leg{50, 9, 2, Place::Backpack, 1, 1}, topaz1{51, 10, 2, Place::PersonalStash, 0, 0}, topaz2{52, 10, 2, Place::PersonalStash, 1, 0};
	auto cubed = [](Item item) { item.place = Place::Cube; return item; };
	auto backpacked = [](Item item) { item.place = Place::Backpack; return item; };
	auto into = Moves({ruby, leg}, {ruby, cubed(leg)}, 0);
	Check(into.size() == 1 && into[0].item.id == 50 && into[0].from == Place::Backpack && into[0].item.place == Place::Cube,
		"X sends the backpack's item into the cube");
	auto out = Moves({ruby, cubed(leg)}, {ruby, backpacked(leg)}, 0);
	Check(out.size() == 1 && out[0].from == Place::Cube && out[0].item.place == Place::Backpack, "and from the cube into the backpack");
	into = Moves({topaz1, topaz2}, {cubed(topaz1), cubed(topaz2)}, 0);
	Check(into.size() == 2 && into[0].from == Place::PersonalStash, "Y sends gems from a stash tab into the cube, each named by where it was");
	Check(Moves({ruby, leg}, {ruby, cubed(leg)}, 50).empty(), "an item placed from the hand is the hand's own notice");
	Item shifted = cubed(leg);
	shifted.x = 2;
	Check(Moves({cubed(leg)}, {shifted}, 0).empty() && Moves({}, {cubed(leg)}, 0).empty(), "an item moved within the cube or first seen there is no move");
	Check(Moves({ruby}, {ruby}, 0).empty(), "nothing changed, nothing moved");

	// A transmute uses the cube's items up and leaves a new one.
	const Item flawed{60, 11, 2, Place::Cube, 0, 0};
	const std::vector<Item> inCube{cubed(topaz1), cubed(topaz2), cubed({53, 10, 2, Place::Cube, 2, 0})};
	Check(NewInCube(inCube, {flawed}, 0).size() == 1 && NewInCube(inCube, {flawed}, 0)[0].id == 60, "the result is new in the cube");
	Check(CubeConsumed(inCube, {flawed}, 0) == 3, "its three ingredients are gone");
	Check(NewInCube(inCube, inCube, 0).empty() && CubeConsumed(inCube, inCube, 0) == 0, "an unchanged cube has neither");
	Check(NewInCube({}, {flawed}, 60).empty(), "the item put down from the hand is not a result");
	Check(CubeConsumed({cubed(topaz1)}, {}, 51) == 0, "an ingredient picked up into the hand was not used up");
	Check(CubeConsumed({cubed(topaz1)}, {topaz1}, 0) == 0, "nor was one sent to a stash");
	Check(NewInCube({ruby}, {ruby, backpacked(flawed)}, 0).empty(), "a new item outside the cube is not the cube's");

	// Three topazes one press moved are one phrase.
	const auto groups = Grouped({topaz1, topaz2, leg});
	Check(groups.size() == 2 && groups[0].count == 2 && groups[0].item.id == 51 && groups[1].count == 1, "like items are grouped, in the order first seen");
	Check(Grouped({}).empty(), "no items, no groups");
	Passed();
}
