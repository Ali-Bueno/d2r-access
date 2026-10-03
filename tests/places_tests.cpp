#include "places.h"
#include "rooms.h"
#include "test_support.h"

#include <set>
#include <vector>

using namespace D2RAccess;
using Generated::PlaceFeature;
using Generated::PlaceInterior;
using Generated::PlaceSeed;
using Generated::PlaceVolume;
using Generated::PresetPlace;

namespace {
// A small floor plan: '#' is wall, anything else is floor.
struct Plan {
	std::vector<const char*> rows;
	bool operator()(int x, int y) const {
		if (y < 0 || y >= static_cast<int>(rows.size())) return false;
		const std::string_view row = rows[static_cast<size_t>(y)];
		return x >= 0 && x < static_cast<int>(row.size()) && row[static_cast<size_t>(x)] != '#';
	}
};

PlaceSeed Point(int x, int y, PlaceFeature feature) {
	return {static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(x + 1), static_cast<uint16_t>(y + 1), feature};
}

// The pieces the generator wrote from the game's own HD layouts.
void GeneratedData() {
	const auto* lut = Places::Find("Act2/Town/LutW.ds1");
	Check(lut != nullptr, "Lut Gholein's west layout has places, found by the path the game keeps");
	Check(Places::Find("ACT2\\TOWN\\LUTW.DS1") == lut, "the lookup ignores case and slash direction");
	Check(Places::Find("data\\global\\tiles\\Act2\\Town\\LutW.ds1") == lut,
		"the tiles folder the game prefixes when it loads LvlPrest is not part of the key");
	Check(Places::Find("Act2/Town/Nowhere.ds1") == nullptr, "a piece the HD layouts never mark has none");
	const auto piece = Places::PieceOf(*lut);
	std::set<PlaceInterior> kinds;
	for (const auto& volume : piece.volumes) kinds.insert(volume.interior);
	Check(kinds.contains(PlaceInterior::Tavern) && kinds.contains(PlaceInterior::Palace), "the tavern and the palace hall are marked");
	Check(piece.volumes.size() == kinds.size(), "a tavern's acoustic zone and lighting box are merged into one room");
	const int tavern = Places::InteriorAt(piece, 140, 40);
	Check(tavern != Places::None && piece.volumes[static_cast<size_t>(tavern)].interior == PlaceInterior::Tavern,
		"the middle of Atma's hall is inside the tavern");
	Check(Places::InteriorAt(piece, 10, 10) == Places::None, "the town's west wall is in no interior");
	bool kitchen = false, bar = false;
	for (const auto& seed : piece.seeds) {
		kitchen |= seed.feature == PlaceFeature::Kitchen;
		bar |= seed.feature == PlaceFeature::Bar;
	}
	Check(kitchen && bar, "the tavern's kitchen and bar props are landmarks");
	Check(Places::NameOf(28) == PresetPlace::Bridge, "LvlPrest 28 is Act 1 - Bridge");
	Check(Places::NameOf(108) == PresetPlace::Graveyard, "LvlPrest 108 is Act 1 - Graveyard");
	Check(Places::NameOf(1) == PresetPlace::None, "a town is a whole level, not a named piece");
}

// Walking distance, not straight-line distance, decides the landmark.
void WallsPartLandmarks() {
	const Plan plan{{
		".....#....",
		".....#....",
		".....#....",
		".....#....",
		"..........",
	}};
	const PlaceSeed seeds[]{Point(0, 2, PlaceFeature::Kitchen), Point(7, 2, PlaceFeature::Bar)};
	const Places::Piece piece{seeds, {}};
	const auto near = Places::Nearest(piece, 0, 0, 4, 2, Places::None, plan, 7);
	Check(near.kind == PlaceFeature::Kitchen && near.steps == 3,
		"three steps to the kitchen beat a bar two cells away behind a wall");
	Check(Places::Nearest(piece, 0, 0, 4, 2, Places::None, plan, 2).seed == Places::None,
		"nothing within the radius is no landmark at all");
	const auto east = Places::Nearest(piece, 0, 0, 7, 3, Places::None, plan, 7);
	Check(east.kind == PlaceFeature::Bar && east.steps == 0, "standing beside a prop is being at it");
	const auto shifted = Places::Nearest(piece, 100, 50, 104, 52, Places::None,
		[&plan](int x, int y) { return plan(x - 100, y - 50); }, 7);
	Check(shifted.kind == PlaceFeature::Kitchen, "seeds are local to the piece and follow its origin");
}

// A forge is a yard, not a point: anywhere on its footprint is at the forge, and
// so is a tile around it; two tiles off is not.
void Footprints() {
	const Plan field{{
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
	}};
	const PlaceSeed forge[]{{2, 0, 22, 4, PlaceFeature::Forge}};
	const Places::Piece piece{forge, {}};
	constexpr int Reach = 5;
	Check(Places::Nearest(piece, 0, 0, 3, 2, Places::None, field, Reach).kind == PlaceFeature::Forge,
		"the far end of the forge yard, eighteen cells from its east edge, is the forge");
	const auto beside = Places::Nearest(piece, 0, 0, 26, 2, Places::None, field, Reach);
	Check(beside.kind == PlaceFeature::Forge && beside.steps == 4, "walking past a tile away is at the forge");
	Check(Places::Nearest(piece, 0, 0, 33, 2, Places::None, field, Reach).seed == Places::None,
		"two tiles off it is not");
}

// A prop standing in a blocked cell, a stool against the counter, is reached
// from the floor around it; at equal distance the earlier kind wins.
void BlockedPropsAndTies() {
	const Plan plan{{
		".....",
		"..#..",
		".....",
	}};
	const PlaceSeed stool[]{Point(2, 1, PlaceFeature::Bar)};
	Check(Places::Nearest({stool, {}}, 0, 0, 0, 1, Places::None, plan, 5).kind == PlaceFeature::Bar,
		"a prop on a blocked cell is reached from its neighbours");
	const PlaceSeed tie[]{Point(0, 0, PlaceFeature::Tables), Point(4, 0, PlaceFeature::Kitchen)};
	Check(Places::Nearest({tie, {}}, 0, 0, 2, 2, Places::None, plan, 5).kind == PlaceFeature::Kitchen,
		"two landmarks at the same distance: the earlier kind in the enum is spoken");
}

// Inside, only the interior's props count; outside, only the street's.
void InteriorsKeepTheirProps() {
	const Plan plan{{"..........", "..........", ".........."}};
	const PlaceSeed seeds[]{Point(1, 1, PlaceFeature::Kitchen), Point(8, 1, PlaceFeature::Stalls)};
	const PlaceVolume hall[]{{0, 0, 5, 3, PlaceInterior::Tavern}};
	const Places::Piece piece{seeds, hall};
	const int inside = Places::InteriorAt(piece, 4, 1);
	Check(inside == 0, "the hall holds its own floor");
	Check(Places::Nearest(piece, 0, 0, 4, 1, inside, plan, 9).kind == PlaceFeature::Kitchen,
		"inside, the kitchen is found");
	Check(Places::Nearest(piece, 0, 0, 5, 1, Places::None, plan, 9).kind == PlaceFeature::Stalls,
		"one step outside, the stalls: the kitchen behind the door is not the street's");
	const PlaceVolume nested[]{{0, 0, 10, 3, PlaceInterior::Temple}, {2, 0, 4, 3, PlaceInterior::Hut}};
	Check(Places::InteriorAt({seeds, nested}, 3, 1) == 1, "the smallest interior holding the point wins");
}

Places::Spot At(uint64_t map, int interior, PlaceFeature feature, PresetPlace preset = PresetPlace::None) {
	Places::Spot spot;
	spot.map = map;
	spot.preset = preset;
	spot.interior = interior;
	spot.feature = feature;
	spot.valid = true;
	return spot;
}

Places::Spot In(Passages::Shape shape, bool deadEnd = false) {
	Places::Spot spot = At(1, Places::None, PlaceFeature::Count);
	spot.passage = shape;
	spot.deadEnd = deadEnd;
	return spot;
}

void CorridorsSpoken() {
	using Passages::Shape;
	Check(Places::Compare(In(Shape::None), In(Shape::Corridor)).corridor, "out of a room into a corridor: corridor");
	const auto junction = Places::Compare(In(Shape::Corridor), In(Shape::Junction));
	Check(junction.node && !junction.corridor, "reaching a junction: junction");
	Check(Places::Compare(In(Shape::Corridor), In(Shape::Bend)).node, "reaching a bend: bend");
	const auto arm = Places::Compare(In(Shape::Junction), In(Shape::Corridor));
	Check(arm.branch && !arm.corridor, "into an arm of the junction: spoken only with news");
	Check(Places::Compare(In(Shape::Corridor), In(Shape::Corridor, true)).corridor,
		"a corridor found shut once its end streams in is spoken again, as a dead end");
	Check(!Places::Compare(In(Shape::Corridor), In(Shape::None)).Any(), "leaving a corridor for a room says nothing");
	Check(!Places::Compare(In(Shape::Junction), In(Shape::Junction)).Any(), "standing in a junction says it once");
}

void WhatIsSpoken() {
	const auto first = Places::Compare({}, At(1, 0, PlaceFeature::Bar));
	Check(first.interior && first.feature && !first.preset && !first.outside, "walking into the tavern: tavern, bar");
	const auto kitchen = Places::Compare(At(1, 0, PlaceFeature::Bar), At(1, 0, PlaceFeature::Kitchen));
	Check(kitchen.feature && !kitchen.interior, "moving on inside: only the new landmark");
	const auto out = Places::Compare(At(1, 0, PlaceFeature::Kitchen), At(1, Places::None, PlaceFeature::Stalls));
	Check(out.outside && out.feature && !out.interior, "out of the door: outside, stalls");
	const auto again = Places::Compare(At(1, Places::None, PlaceFeature::Stalls), At(1, 0, PlaceFeature::Stalls));
	Check(again.interior && again.feature, "the same kind of landmark in a new room is named with it");
	const auto bridge = Places::Compare(At(0, Places::None, PlaceFeature::Count), At(7, Places::None, PlaceFeature::River, PresetPlace::Bridge));
	Check(bridge.preset && bridge.feature, "a named piece is announced on entering it");
	const auto same = Places::Compare(At(7, Places::None, PlaceFeature::River, PresetPlace::Bridge),
		At(7, Places::None, PlaceFeature::Count, PresetPlace::Bridge));
	Check(!same.Any(), "walking on along the bridge says nothing");
	const auto otherBridge = Places::Compare(At(7, Places::None, PlaceFeature::Count, PresetPlace::Bridge),
		At(8, Places::None, PlaceFeature::Count, PresetPlace::Bridge));
	Check(!otherBridge.preset, "the next piece of the same river or bridge is not named again");
	const auto ford = Places::Compare(At(8, Places::None, PlaceFeature::Count, PresetPlace::River),
		At(9, Places::None, PlaceFeature::Count, PresetPlace::Bridge));
	Check(ford.preset, "a bridge over the river is named");
	Check(Places::MapKey(28, 10, 20) != Places::MapKey(28, 10, 21), "two placements of one piece are told apart");
}

void CarryAndHysteresis() {
	const auto bar = At(1, 0, PlaceFeature::Bar);
	Check(Places::Carry(bar, At(1, 0, PlaceFeature::Count)) == bar, "the floor between two landmarks keeps the last one");
	Check(Places::Carry(bar, At(1, Places::None, PlaceFeature::Count)).feature == PlaceFeature::Count,
		"leaving the room drops its landmark");
	Rooms::BasicHysteresis<Places::Spot> gate;
	Check(gate.Observe(bar, 0), "the first place is spoken at once");
	bool spoke = false;
	for (int now = 100; now <= 900; now += 200) {
		spoke |= gate.Observe(At(1, 0, PlaceFeature::Kitchen), now);
		spoke |= gate.Observe(bar, now + 100);
	}
	Check(!spoke && gate.current == bar, "standing between the bar and the kitchen does not chatter");
}
}

int main() {
	GeneratedData();
	WallsPartLandmarks();
	Footprints();
	BlockedPropsAndTies();
	InteriorsKeepTheirProps();
	WhatIsSpoken();
	CorridorsSpoken();
	CarryAndHysteresis();
	Passed();
}
