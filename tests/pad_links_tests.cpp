#include "pad_links.h"
#include "test_support.h"

#include <vector>

using namespace D2RAccess;
using LevelRoute::Destination;
using LevelRoute::Link;
using LevelRoute::Pad;
using LevelRoute::Pads;

namespace {
// The Arcane Sanctuary: level 74 at tile 5000,1000, rooms of 12x12 tiles.
NativeRooms::Room Room(int column, int row) {
	const int x = 5000 + column * 12, y = 1000 + row * 12;
	return {x, y, 12, 12, NativeRooms::Identity(x, y)};
}
const std::vector<NativeRooms::Room> Rooms{Room(1, 6), Room(2, 6), Room(1, 7), Room(1, 8), Room(7, 5)};

bool Leads(const Pads& pads, Pad from, Pad to, bool seen = false) {
	const auto link = pads.Lead(from, Rooms);
	return link && link->from == from && link->to == to && link->seen == seen;
}
bool LeadsNowhere(const Pads& pads, Pad from) { return !pads.Lead(from, Rooms); }

// 2026-10-02, the room at 25060,5420 (Act2/Arcane/sanctNSW2.ds1): three pairs,
// each the file's pair of one class, and each measured both ways in the log.
const Pad West{25066, 5427, 192}, WestSouth{25066, 5467, 192};
const Pad Upper{25107, 5441, 306}, Lower{25097, 5456, 306};
const Pad Near{25097, 5446, 305}, Far{25107, 5466, 305};
// The room at 25120,5360 (sanctEW2.ds1): a pad on the floor east of the gap
// and one at the west walkway's end, on shut ground.
const Pad Floor{25156, 5387, 192}, Edge{25145, 5387, 192};

void ThreePairsInOneRoom() {
	Pads pads;
	for (const auto& pad : {West, WestSouth, Upper, Lower, Near, Far}) Check(pads.Note(pad), "a pad not seen before is new");
	Check(!pads.Note(Lower) && pads.All().size() == 6, "a pad seen again, under a new unit id, is the same pad");
	Check(Leads(pads, West, WestSouth) && Leads(pads, WestSouth, West), "25066,5427 and 25066,5467 lead to each other, as measured");
	Check(Leads(pads, Upper, Lower) && Leads(pads, Lower, Upper), "25107,5441 and 25097,5456 lead to each other, as measured");
	Check(Leads(pads, Near, Far) && Leads(pads, Far, Near), "25097,5446 and 25107,5466 lead to each other, as measured");
	Check(pads.Links(Rooms).size() == 6, "every pad of the room has its way");
	Check(!Leads(pads, Lower, Near), "the nearest pad is not where one leads: 25097,5456 is ten cells from 25097,5446");
}

void FloorAndEdge() {
	Pads pads;
	pads.Note(Floor);
	Check(LeadsNowhere(pads, Floor), "a pad whose partner is not seen yet leads nowhere known");
	pads.Note(Edge);
	Check(Leads(pads, Floor, Edge) && Leads(pads, Edge, Floor), "the pad at the walkway's end leads back across the gap");
}

void NoGuessing() {
	Pads pads;
	pads.Note(Floor);
	pads.Note(Edge);
	const Pad third{25130, 5400, 192};
	pads.Note(third);
	Check(LeadsNowhere(pads, Floor) && LeadsNowhere(pads, Edge) && LeadsNowhere(pads, third),
		"with three of a class in a room the game's pick depends on an order not read: none is assumed");
	Check(pads.Leads(Floor, Rooms) == Destination::Unknown, "nor is it said whether such a pad leads where the player has stood");
	const Pad alone{25087, 5386, 304};
	pads.Note(alone);
	Check(LeadsNowhere(pads, alone), "a pad alone of its class in its room leads nowhere known");
	const Pad other{25070, 5400, 304}; // the same class in the same room, the one at 25060,5360
	pads.Note(other);
	Check(Leads(pads, alone, other), "the other pad of the class in the same room");
	const Pad outside{24990, 5400, 304};
	pads.Note(outside);
	Check(LeadsNowhere(pads, outside), "a pad in no room of the level leads nowhere known");
	Check(pads.Leads(outside, Rooms) == Destination::Unknown, "and is not said to lead anywhere");
	Check(!pads.Note(Pad{25100, 5400, 0}), "an object of no pad class is no pad");
}

void SeenWins() {
	Pads pads;
	for (const auto& pad : {West, WestSouth, Upper, Lower}) pads.Note(pad);
	const auto before = pads.Revision();
	Pad to;
	Check(pads.Used(West, {25066.4, 5466.0}, to) && to == WestSouth && Leads(pads, West, WestSouth, true),
		"a use that lands beside the pad the rule names is kept as seen");
	Check(pads.Revision() == before + 1, "a use learned is a change");
	Check(pads.Used(West, {25066.4, 5466.0}, to) && pads.Revision() == before + 1, "the same use again changes nothing");
	Check(pads.Used(West, {25108.0, 5443.0}, to) && to == Upper && Leads(pads, West, Upper, true),
		"what a pad was seen to do wins over the rule");
	Check(Leads(pads, WestSouth, West), "the way back keeps the rule until it is used");
	Check(!pads.Used(Upper, {25200.0, 5300.0}, to), "a landing beside no pad known teaches nothing");
	Check(!pads.Used(Upper, {25107.0, 5441.0}, to), "the pad used is never where it leads");
	Pads fresh;
	fresh.Note(Edge);
	Check(fresh.Used(Floor, {25143.0, 5390.0}, to) && to == Edge && fresh.All().size() == 2,
		"landing 2026-10-02 08:50:13 at 25143,5390 is beside the edge pad, and the pad used is kept");
}

// 2026-11-03 05:32-05:50, the user on a controller: the pair at 25452,5331 and
// 25477,5341 crossed seven times, every pad heard alike.
void WhereEachLeads() {
	const Pad Hub{25452, 5331, 304}, Across{25477, 5341, 304};
	Pads pads;
	pads.Note(Hub);
	Check(pads.Leads(Hub, Rooms) == Destination::New, "a pad whose partner was never seen leads somewhere new: the player was never near it");
	pads.Note(Across);
	Check(pads.Leads(Hub, Rooms) == Destination::New && pads.Leads(Across, Rooms) == Destination::New, "neither pad stood at yet");
	Check(pads.Stood({25450.6, 5332.2}) && pads.Been(Hub) && !pads.Been(Across), "standing at a pad marks that pad alone");
	Check(pads.Leads(Across, Rooms) == Destination::Been && pads.Leads(Hub, Rooms) == Destination::New,
		"the pad across leads back where the player stands; this one still somewhere new");
	Check(!pads.Stood({25450.6, 5332.2}), "standing there again is nothing new");
	Check(!pads.Stood({25465.0, 5336.0}), "farther than a landing from both, none is marked");
	Check(pads.Nearest({25478.0, 5343.0}, LevelRoute::LandNear) == Across && !pads.Nearest({25465.0, 5336.0}, LevelRoute::LandNear),
		"the pad nearest a place, within a landing of it");
	Pad to;
	Check(pads.Used(Hub, {25478.0, 5343.0}, to) && to == Across && pads.Been(Across), "a pad taken marks the pad it put the player beside");
	Check(pads.Leads(Hub, Rooms) == Destination::Been && pads.Leads(Across, Rooms) == Destination::Been,
		"after one crossing both lead where the player has been: the loop is heard");
	Pads taken;
	taken.Note(Across);
	Check(taken.Used(Hub, {25478.0, 5343.0}, to) && taken.Been(Hub) && taken.Been(Across), "a pad taken from afar marks both ends");
}
}

int main() {
	ThreePairsInOneRoom();
	FloorAndEdge();
	NoGuessing();
	SeenWins();
	WhereEachLeads();
	Passed();
	return 0;
}
