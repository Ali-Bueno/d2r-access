#pragma once

#include "generated/place_data.h"
#include "passages.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

// Named places inside one map piece: the piece's own LvlPrest name, the interior
// the game's HD layout marks (an acoustic or lighting volume), and the landmark
// prop nearest the player by walking distance. Pure logic, no native calls.
// docs/places.md
namespace D2RAccess::Places {
using Generated::PlaceFeature;
using Generated::PlaceInterior;
using Generated::PresetPlace;

inline constexpr int None = -1;

// The generated places of a DS1 path as the game keeps it
// ("data\global\tiles\Act2\Town\LutW.ds1"), or null when the HD layout marks nothing.
const Generated::PresetPlaces* Find(std::string_view ds1Path);
PresetPlace NameOf(int32_t def);

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
	int          interior = None; // index into the piece's volumes
	PlaceFeature feature = PlaceFeature::Count;
	Passages::Shape passage = Passages::Shape::None;
	bool         deadEnd = false;
	bool         valid = false;
	bool operator==(const Spot&) const = default;
};
uint64_t MapKey(int32_t def, int tileX, int tileY);
// A landmark the scanner lists too, not only the walk-in announcer: a way across a
// level that has no unit, such as the desert's cliff stairs (a tester, 2026-10-17).
bool Tracked(PlaceFeature kind);

// No landmark in reach inside the same interior keeps the last one: the floor
// between two landmarks is not leaving either.
Spot Carry(const Spot& current, Spot observed);

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
