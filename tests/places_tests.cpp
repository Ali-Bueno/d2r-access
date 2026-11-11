#include "places.h"
#include "rooms.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
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
	Check(Places::NameOf(47) == PresetPlace::Cottage, "LvlPrest 47 is Act 1 - Cottages 1, one cottage");
	Check(Places::NameOf(47, "Act1/Outdoors/Cott6.ds1") == PresetPlace::Stable,
		"the row's sixth piece is a stable");
	Check(Places::NameOf(49, "Act1/Outdoors/Cott7.ds1") == PresetPlace::BurningCottage,
		"Cott7 is the burning cottage");
	Check(Places::NameOf(47, "Act1/Outdoors/Cott2.ds1") == PresetPlace::Cottage,
		"another piece of the row keeps the row's name");
	Check(Places::NameOf(47, "Act1/Outdoors/Cott5.ds1") == PresetPlace::CottageWithPorch
		&& Places::NameOf(48, "data\\global\\tiles\\Act1\\Outdoors\\Cott5A.ds1") == PresetPlace::CottageWithPorch,
		"Cott5 and Cott5A are the cottage with a porch (a tester's screenshots, 2026-10-18)");
	Check(Places::NameOf(47, "Act1/Outdoors/Cott1.ds1") == PresetPlace::Cottage
		&& Places::NameOf(48, "data\\global\\tiles\\Act1\\Outdoors\\Cott1A.ds1") == PresetPlace::Cottage,
		"Cott1 and Cott1A are the plain stone cottage (a tester, 2026-10-19)");
	Check(Places::Find("Act1/Outdoors/Cott6A.ds1") == nullptr && Places::Find("Act1/Outdoors/Cott6.ds1") == nullptr,
		"the stable's cottage visbox does not make it an interior");
	const auto* porch5 = Places::Find("Act1/Outdoors/Cott5.ds1");
	const auto* porch5a = Places::Find("Act1/Outdoors/Cott5A.ds1");
	Check((!porch5 || Places::PieceOf(*porch5).volumes.empty()) && (!porch5a || Places::PieceOf(*porch5a).volumes.empty()),
		"the porch cottage has no interior word of its own: its name is the only one said");
	const auto* cottage = Places::Find("Act1/Outdoors/Cott4.ds1");
	Check(cottage && Places::PieceOf(*cottage).volumes.size() == 1
		&& Places::PieceOf(*cottage).volumes[0].interior == PlaceInterior::Cottage, "a cottage keeps its interior");
	Check(Places::Find("Act2/Sewer/Sews.ds1") == nullptr, "the sewers' crate piles named table01/02 are not tables (a tester, 2026-10-18)");
	const auto grateAt = [](const char* file, int x, int y) {
		const auto* piece = Places::Find(file);
		if (!piece) return false;
		const auto seeds = Places::PieceOf(*piece).seeds;
		return std::any_of(seeds.begin(), seeds.end(), [&](const PlaceSeed& seed) {
			return seed.feature == PlaceFeature::SewerGrate && (x < 0 || (seed.minX == x && seed.minY == y)); });
	};
	Check(grateAt("Act2/Sewer/SewNSW.ds1", 37, 33), "the sewers' round grate (manhole01) is a landmark (a tester, 2026-10-19)");
	Check(!grateAt("Act2/Sewer/SewS2.ds1", -1, -1), "the flat water channel grates are not the round grate");
	Check(Places::Padding(PresetPlace::Tower) > 0 && Places::Padding(PresetPlace::Cottage) == 0, "only the tower ruin is named from afar");
	Check(Places::NameOf(163) == PresetPlace::Tower && Places::NameOf(162) == PresetPlace::None,
		"the Black Marsh's tower is a tower ruin, the Moldy Tome's piece is not (a tester, 2026-10-23)");
	const auto bridgeAt = [](const char* file) {
		const auto* piece = Places::Find(file);
		if (!piece) return false;
		const auto seeds = Places::PieceOf(*piece).seeds;
		return std::any_of(seeds.begin(), seeds.end(), [](const PlaceSeed& seed) {
			return seed.feature == PlaceFeature::Bridge && seed.maxX - seed.minX > 1 && seed.maxY - seed.minY > 1; });
	};
	Check(bridgeAt("Act3/Jungle/BogN.ds1"), "a jungle log over the river is a bridge, as long as the log (a tester, 2026-10-23)");
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
	const PlaceSeed star[]{{2, 0, 4, 2, PlaceFeature::FireStar}};
	const auto across = Places::Nearest(Places::Piece{star, {}}, 0, 0, 33, 2, Places::None, field, Reach);
	Check(across.kind == PlaceFeature::FireStar && across.steps == 29,
		"the burning star is told from across its piece, not on its points (a tester, 2026-10-19)");
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
	spot.namedMap = preset == PresetPlace::None ? 0 : map;
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
	const auto cottageA = At(10, Places::None, PlaceFeature::Count, PresetPlace::Cottage);
	auto between = At(0, Places::None, PlaceFeature::Count, PresetPlace::Cottage); // plain ground, the name held
	between.namedMap = cottageA.namedMap;
	Check(!Places::Compare(cottageA, between).preset, "leaving a cottage onto the ground around it says nothing");
	Check(Places::Compare(between, At(11, Places::None, PlaceFeature::Count, PresetPlace::Cottage)).preset,
		"the next cottage along is named again (a tester, 2026-10-18)");
	Check(!Places::Compare(between, cottageA).preset, "stepping back into the same cottage's piece is not news");
	Check(Places::Separate(PresetPlace::Stable) && !Places::Separate(PresetPlace::River), "a river is one name, a building is each");
	Check(Places::HoldsName(PresetPlace::River, Places::SeparateHoldMs * 10)
		&& Places::HoldsName(PresetPlace::Cottage, 1000) && !Places::HoldsName(PresetPlace::Cottage, Places::SeparateHoldMs),
		"a building's name lapses, so coming back to it later speaks it again");
	const auto climbed = Places::Compare(At(3, Places::None, PlaceFeature::CliffStairs), At(3, Places::None, PlaceFeature::CliffStairsTop));
	Check(!climbed.feature, "reaching the top of the stairs walked up is not a new landmark");
	const auto arrived = Places::Compare(At(3, Places::None, PlaceFeature::Count), At(3, Places::None, PlaceFeature::CliffStairsTop));
	Check(arrived.feature, "coming to the top landing from the upper level names the stairs");
	const auto top = At(3, Places::None, PlaceFeature::CliffStairsTop);
	Check(Places::HeldStairway(top, Places::StairwayHoldMs - 1) == top, "a stairway just left still holds");
	const auto back = Places::Compare(Places::HeldStairway(top, Places::StairwayHoldMs), top);
	Check(back.feature, "walking away and back to the top names the stairs again");
	Check(Places::Tracked(PlaceFeature::CliffStairs) && Places::Tracked(PlaceFeature::CliffStairsTop), "both landings are in the scanner");
	Check(Places::Spoken(PlaceFeature::CliffStairs) == PlaceFeature::Stairs && Places::Spoken(PlaceFeature::CliffStairsTop) == PlaceFeature::Stairs
		&& Places::Spoken(PlaceFeature::Forge) == PlaceFeature::Forge, "either landing is said as the stairs (a tester, 2026-10-22)");
}

// The scanner lists a cliff stairway once, halfway between its landings (a tester, 2026-10-22:
// the route to the top landing was lost on the stairs' bend).
void StairwaysListedOnce() {
	const auto* cliff = Places::Find("Act2/BigCliff/CliffLP.ds1");
	Check(cliff != nullptr, "the Dry Hills' cliff piece has its stairs");
	const auto listed = Places::Listings(Places::PieceOf(*cliff));
	Check(listed.size() == 1 && listed[0].kind == PlaceFeature::Stairs, "its two landings are one listing, the stairs");
	const auto piece = Places::PieceOf(*cliff);
	const auto centre = [](const PlaceSeed& seed) { return std::pair{(seed.minX + seed.maxX) / 2.0, (seed.minY + seed.maxY) / 2.0}; };
	const auto [bx, by] = centre(piece.seeds[0]);
	const auto [tx, ty] = centre(piece.seeds[1]);
	Check(!listed.empty() && listed[0].x == (bx + tx) / 2 && listed[0].y == (by + ty) / 2, "halfway between the landings");
	Check(!listed.empty() && listed[0].side >= static_cast<int>(std::hypot(bx - tx, by - ty)), "as wide as the way from one to the other");

	const PlaceSeed lone[] = {Point(4, 4, PlaceFeature::CliffStairsTop), Point(9, 9, PlaceFeature::Forge)};
	const auto alone = Places::Listings({lone, {}});
	Check(alone.size() == 1 && alone[0].kind == PlaceFeature::Stairs && alone[0].x == 4.5, "a landing without its pair is still listed");

	// At the basin, the DS1's BloodFountain object (68,71 and 71,63), not her chest's dais
	// (36,71 and 66,31): a tester, 2026-10-22, was led to the chest.
	const struct { const char* path; double x, y; } basins[] = {
		{"Act1/Crypt/CryptCountess1.ds1", 68.5, 71.5}, {"Act1/Crypt/CryptCountess2.ds1", 71.5, 63.5}};
	for (const auto& basin : basins) {
		const auto* countess = Places::Find(basin.path);
		const auto cisterns = countess ? Places::Listings(Places::PieceOf(*countess)) : std::vector<Places::Listing>{};
		Check(cisterns.size() == 1 && cisterns[0].kind == PlaceFeature::BloodCistern && cisterns[0].side == 3,
			"the Countess's cistern is listed in both her layouts, as wide as the basin (3x3)");
		Check(!cisterns.empty() && cisterns[0].x == basin.x && cisterns[0].y == basin.y, "at the basin, not the chest's dais");
	}
	const auto* tristram = Places::Find("Act1/Tristram/Tri_Town4.ds1");
	const auto fountains = tristram ? Places::Listings(Places::PieceOf(*tristram)) : std::vector<Places::Listing>{};
	Check(fountains.size() == 1 && fountains[0].kind == PlaceFeature::BloodFountain,
		"Tristram's fountain is listed, a way out of its houses (a tester, 2026-10-22)");
	const auto* copy = Places::Find("Act1/Crypt/CryptNWarpNext.ds1");
	Check(!copy || Places::Listings(Places::PieceOf(*copy)).empty(), "the unused copy of her room is not");

	for (const char* town : {"Act2/Town/LutW.ds1", "Act2/Town/LutN.ds1"}) {
		const auto* lut = Places::Find(town);
		const auto listed = lut ? Places::Listings(Places::PieceOf(*lut)) : std::vector<Places::Listing>{};
		const auto palace = std::find_if(listed.begin(), listed.end(), [](const Places::Listing& listing) {
			return listing.interior == PlaceInterior::Palace; });
		Check(listed.size() == 1 && palace != listed.end() && palace->side >= 20,
			"Lut Gholein's palace entrance is listed in both layouts, as wide as its hall (a tester, 2026-10-23)");
	}

	// The Chaos Sanctuary's pentagram, centred where Diablo appears (the DS1's DiabloStart at 53,53).
	const auto* heart = Places::Find("Act4/Diab/Heart.ds1");
	const auto stars = heart ? Places::Listings(Places::PieceOf(*heart)) : std::vector<Places::Listing>{};
	Check(stars.size() == 1 && stars[0].kind == PlaceFeature::Pentagram && stars[0].x == 53 && stars[0].y == 53,
		"the pentagram at the Chaos Sanctuary's heart is listed where Diablo appears (a tester, 2026-10-27)");
	for (const char* forge : {"Act4/Lava/ForgeE.ds1", "Act4/Lava/ForgeW.ds1"}) {
		const auto* lava = Places::Find(forge);
		const auto piece = lava ? Places::PieceOf(*lava) : Places::Piece{};
		Check(std::any_of(piece.seeds.begin(), piece.seeds.end(), [](const auto& seed) { return seed.feature == PlaceFeature::Hellforge; })
			&& Places::Listings(piece).empty(), "the Hellforge is a landmark in both its layouts, listed by its own object only");
	}
	// Travincal: the altars and blood pits of the central square, whose corner braziers are the
	// DS1's FloorBrazier objects at 60,20 to 85,45; not the temple's hall north of it.
	const auto* square = Places::Find("Act3/Travincal/TravS.ds1");
	const auto altars = square ? Places::Listings(Places::PieceOf(*square)) : std::vector<Places::Listing>{};
	Check(altars.size() == 1 && altars[0].kind == PlaceFeature::CouncilAltar && altars[0].x > 70 && altars[0].x < 76
		&& altars[0].y > 30 && altars[0].y < 35, "the Council's altar is listed in Travincal's central square (a tester, 2026-11-11)");
	const auto* temple = Places::Find("Act3/Travincal/TravN.ds1");
	Check(temple && Places::Listings(Places::PieceOf(*temple)).empty(), "and nowhere else in Travincal");
}

// The Pandemonium Fortress's hall, where the stash and Tyrael are, is inside; the courtyard
// of the waypoint east of its colonnade is not (a tester's log, 2026-10-27: local 26,41 was "outside").
void FortressHall() {
	const auto* fortress = Places::Find("Act4/Fort/Fortress.ds1");
	Check(fortress != nullptr, "the Fortress has places");
	if (!fortress) return;
	const auto piece = Places::PieceOf(*fortress);
	const int stash = Places::InteriorAt(piece, 19, 40);
	Check(stash != Places::None && piece.volumes[static_cast<size_t>(stash)].interior == PlaceInterior::Fortress,
		"the stash stands inside the fortress");
	Check(Places::InteriorAt(piece, 26, 41) != Places::None, "so does the nave on the way to it");
	Check(Places::InteriorAt(piece, 48, 43) == Places::None, "the courtyard by the waypoint is outside");
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
	StairwaysListedOnce();
	CorridorsSpoken();
	CarryAndHysteresis();
	FortressHall();
	Passed();
}
