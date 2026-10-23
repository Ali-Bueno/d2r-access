#pragma once

#include "generated/place_data.h"
#include "passages.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

// Named places inside one map piece: the piece's own LvlPrest name, the interior
// the game's HD layout marks (an acoustic or lighting volume), and the landmark
// prop nearest the player by walking distance. Pure logic, no native calls.
// docs/places.md
namespace D2RAccess::Places {
using Generated::PlaceFeature;
using Generated::PlaceInterior;
using Generated::PresetPlace;

inline constexpr int None = -1;
// A held building name lapses this long after the player last stood in its piece.
inline constexpr long long SeparateHoldMs = 15000;
// A stairway's landings are news again this long after the player last stood by one (a tester, 2026-10-18).
inline constexpr long long StairwayHoldMs = SeparateHoldMs;

// The generated places of a DS1 path as the game keeps it
// ("data\global\tiles\Act2\Town\LutW.ds1"), or null when the HD layout marks nothing.
const Generated::PresetPlaces* Find(std::string_view ds1Path);
// A LvlPrest row's place, or the piece's own when the row picks among kinds
// (a cottage row's stable): `ds1Path` as Find takes it.
PresetPlace NameOf(int32_t def, std::string_view ds1Path = {});

// A named place that is one building (a cottage, the tower ruin): the next such piece is named
// again, where a river or a bridge built from a chain of pieces is named once.
bool Separate(PresetPlace place);
// Whether a name last earned `idleMs` ago still holds on plain ground near its piece.
bool HoldsName(PresetPlace place, long long idleMs);
// Tiles around a piece where its name is spoken already; 0 for most places.
int Padding(PresetPlace place);

// One piece's landmarks and interiors, in preset-local subtiles.
struct Piece {
	std::span<const Generated::PlaceSeed>   seeds;
	std::span<const Generated::PlaceVolume> volumes;
};
Piece PieceOf(const Generated::PresetPlaces& places);

// Index into piece.volumes of the smallest interior holding the subtile, or None.
int InteriorAt(const Piece& piece, int x, int y);

// Whether the player can stand on a world subtile.
using Passable = std::function<bool(int x, int y)>;

struct Feature {
	int          seed = None; // index into piece.seeds
	PlaceFeature kind = PlaceFeature::Count;
	int          steps = 0;
};
// The landmark nearest the player by walking distance, at most `radius` steps
// from its footprint (standing on it is zero): walls part two landmarks the way
// they part two rooms. A footprint that blocks (a forge, a stool at the counter)
// is reached from any cell around it. Only props in the player's interior count;
// outside, only props outside every interior. `originX/Y` is the map origin in
// world subtiles.
Feature Nearest(const Piece& piece, int originX, int originY, int playerX, int playerY,
	int interior, const Passable& passable, int radius);

// What the player stands in, as the announcer keys it.
struct Spot {
	uint64_t     map = 0; // which placed map: Def and origin; 0 for ground no map owns
	PresetPlace  preset = PresetPlace::None;
	uint64_t     namedMap = 0; // the map the preset was earned in, held on the plain ground around it
	int          interior = None; // index into the piece's volumes
	PlaceFeature feature = PlaceFeature::Count;
	Passages::Shape passage = Passages::Shape::None;
	bool         deadEnd = false;
	bool         valid = false;
	bool operator==(const Spot&) const = default;
};
uint64_t MapKey(int32_t def, int tileX, int tileY);
// A landmark the scanner lists too, not only the walk-in announcer: a way across a
// level that has no unit, such as the desert's cliff stairs (a tester, 2026-10-17),
// or one that tells the way, the Countess's cistern by her doors (2026-10-22) and
// Tristram's fountain, a way out of its burning houses (2026-10-22).
bool Tracked(PlaceFeature kind);
// The two landings of one cliff stairway: walking from one to the other is not news.
bool Stairway(PlaceFeature kind);
// The landmark a kind is said as: either landing of a cliff stairway is the stairs
// (a tester, 2026-10-22: which end is rarely needed).
PlaceFeature Spoken(PlaceFeature kind);

// A tracked landmark as the scanner lists it, in preset-local subtiles, `side` cells
// across. A cliff stairway is one, halfway between its landings and as wide as the
// way from one to the other, so a route to it ends on the stairs.
struct Listing {
	PlaceFeature kind = PlaceFeature::Count; // as spoken
	double       x = 0, y = 0;
	int          side = 0;
};
std::vector<Listing> Listings(const Piece& piece);

// No landmark in reach inside the same interior keeps the last one: the floor
// between two landmarks is not leaving either.
Spot Carry(const Spot& current, Spot observed);
// The spot to compare against: a stairway left `idleMs` ago no longer holds its landmark.
Spot HeldStairway(Spot current, long long idleMs);

// What changed from `before` to `now`, in the order it is spoken.
struct Change {
	bool preset = false;   // entered a named map piece of another kind
	bool interior = false; // entered an interior
	bool outside = false;  // left an interior for no other
	bool feature = false;  // a new landmark, or the same kind in a new interior
	bool node = false;     // reached a bend or a junction
	bool corridor = false; // walked into a corridor from open floor, or found it shut
	bool branch = false;   // walked into a corridor out of a bend or junction: spoken only with news
	bool Any() const { return preset || interior || outside || feature || node || corridor || branch; }
};
Change Compare(const Spot& before, const Spot& now);
}
