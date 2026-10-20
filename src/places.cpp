#include "places.h"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace D2RAccess::Places {
namespace {

// The game prefixes the tiles folder to the LvlPrest File column when it loads
// the table (seen live: "data\global\tiles\Act2\Town\LutW.ds1").
constexpr std::string_view TilesFolder = "data/global/tiles/";

// The generator keys pieces by the File column itself: lowercase, forward slashes.
std::string Normalize(std::string_view path) {
	std::string key(path);
	for (char& c : key) {
		if (c == '\\') c = '/';
		else if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	}
	if (key.starts_with(TilesFolder)) key.erase(0, TilesFolder.size());
	return key;
}

bool Inside(const Generated::PlaceVolume& volume, int x, int y) {
	return x >= volume.minX && x < volume.maxX && y >= volume.minY && y < volume.maxY;
}

long long Area(const Generated::PlaceVolume& volume) {
	return static_cast<long long>(volume.maxX - volume.minX) * (volume.maxY - volume.minY);
}

// How far a landmark is told from: `radius`, or for one seen across the whole
// map piece it stands in, from anywhere in that piece. The Crypt's burning star
// was told when almost stepping on its points (a tester, 2026-10-19).
int Reach(PlaceFeature kind, int radius) {
	constexpr int AcrossPiece = 40; // subtiles: the 8x8 tiles of a Crypt or Catacombs piece
	return kind == PlaceFeature::FireStar ? std::max(radius, AcrossPiece) : radius;
}

uint64_t CellKey(int x, int y) {
	return static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32 | static_cast<uint32_t>(y);
}
}

const Generated::PresetPlaces* Find(std::string_view ds1Path) {
	const std::string key = Normalize(ds1Path);
	const auto* begin = std::begin(Generated::PresetPlacesTable);
	const auto* end = std::end(Generated::PresetPlacesTable);
	const auto* found = std::lower_bound(begin, end, key,
		[](const Generated::PresetPlaces& entry, const std::string& wanted) { return std::string_view(entry.file) < wanted; });
	return found != end && std::string_view(found->file) == key ? found : nullptr;
}

PresetPlace NameOf(int32_t def, std::string_view ds1Path) {
	if (!ds1Path.empty()) {
		const std::string key = Normalize(ds1Path);
		for (const auto& entry : Generated::PresetFileNames)
			if (key == entry.file) return entry.place;
	}
	const auto* begin = std::begin(Generated::PresetNames);
	const auto* end = std::end(Generated::PresetNames);
	const auto* found = std::lower_bound(begin, end, def,
		[](const Generated::PresetName& entry, int32_t wanted) { return entry.def < wanted; });
	return found != end && found->def == def ? found->place : PresetPlace::None;
}

Piece PieceOf(const Generated::PresetPlaces& places) {
	return {std::span(Generated::PlaceSeeds).subspan(places.firstSeed, places.seedCount),
		std::span(Generated::PlaceVolumes).subspan(places.firstVolume, places.volumeCount)};
}

int InteriorAt(const Piece& piece, int x, int y) {
	int best = None;
	for (size_t i = 0; i < piece.volumes.size(); ++i) {
		const auto& volume = piece.volumes[i];
		if (Inside(volume, x, y) && (best == None || Area(volume) < Area(piece.volumes[static_cast<size_t>(best)])))
			best = static_cast<int>(i);
	}
	return best;
}

Feature Nearest(const Piece& piece, int originX, int originY, int playerX, int playerY,
	int interior, const Passable& passable, int radius) {
	// Every passable cell on or touching an eligible prop's footprint is a goal for
	// that prop's kind; when two kinds claim a cell, the earlier one in the enum wins.
	std::unordered_map<uint64_t, int> goals;
	const auto claim = [&](int x, int y, int seed) {
		if (!passable(x, y)) return;
		auto [at, added] = goals.try_emplace(CellKey(x, y), seed);
		if (!added && piece.seeds[static_cast<size_t>(seed)].feature < piece.seeds[static_cast<size_t>(at->second)].feature)
			at->second = seed;
	};
	int farthest = 0;
	for (size_t i = 0; i < piece.seeds.size(); ++i) {
		const auto& seed = piece.seeds[i];
		const int minX = originX + seed.minX - 1, minY = originY + seed.minY - 1;
		const int maxX = originX + seed.maxX, maxY = originY + seed.maxY; // inclusive: one cell around the footprint
		const int reach = Reach(seed.feature, radius);
		if (playerX < minX - reach || playerX > maxX + reach || playerY < minY - reach || playerY > maxY + reach) continue;
		farthest = std::max(farthest, reach);
		if (InteriorAt(piece, (seed.minX + seed.maxX) / 2, (seed.minY + seed.maxY) / 2) != interior) continue;
		for (int y = minY; y <= maxY; ++y)
			for (int x = minX; x <= maxX; ++x) claim(x, y, static_cast<int>(i));
	}
	if (goals.empty()) return {};
	// Breadth-first over walkable cells, one ring per step; the closest ring with
	// a goal decides, and inside it the enum order does.
	std::unordered_set<uint64_t> seen{CellKey(playerX, playerY)};
	std::vector<std::pair<int, int>> ring{{playerX, playerY}}, next;
	for (int steps = 0; steps <= farthest && !ring.empty(); ++steps) {
		Feature best;
		for (const auto& [x, y] : ring) {
			const auto goal = goals.find(CellKey(x, y));
			if (goal == goals.end()) continue;
			const auto kind = piece.seeds[static_cast<size_t>(goal->second)].feature;
			if (steps > Reach(kind, radius)) continue;
			if (best.seed == None || kind < best.kind) best = {goal->second, kind, steps};
		}
		if (best.seed != None) return best;
		next.clear();
		for (const auto& [x, y] : ring) {
			constexpr int Steps[4][2]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
			for (const auto& step : Steps) {
				const int nx = x + step[0], ny = y + step[1];
				if (passable(nx, ny) && seen.insert(CellKey(nx, ny)).second) next.emplace_back(nx, ny);
			}
		}
		ring.swap(next);
	}
	return {};
}

uint64_t MapKey(int32_t def, int tileX, int tileY) {
	return static_cast<uint64_t>(static_cast<uint32_t>(def)) << 32
		| static_cast<uint64_t>(static_cast<uint16_t>(tileX)) << 16 | static_cast<uint16_t>(tileY);
}

bool Separate(PresetPlace place) {
	return place == PresetPlace::Cottage || place == PresetPlace::Stable || place == PresetPlace::BurningCottage
		|| place == PresetPlace::CottageWithPorch || place == PresetPlace::Tower;
}

bool HoldsName(PresetPlace place, long long idleMs) { return !Separate(place) || idleMs < SeparateHoldMs; }

int Padding(PresetPlace place) {
	for (const auto& pad : Generated::PresetPads)
		if (pad.place == place) return pad.tiles;
	return 0;
}

bool Tracked(PlaceFeature kind) { return Stairway(kind); }
bool Stairway(PlaceFeature kind) { return kind == PlaceFeature::CliffStairs || kind == PlaceFeature::CliffStairsTop; }

Spot Carry(const Spot& current, Spot observed) {
	if (current.valid && observed.feature == PlaceFeature::Count && observed.map == current.map
		&& observed.interior == current.interior) observed.feature = current.feature;
	return observed;
}

Spot HeldStairway(Spot current, long long idleMs) {
	if (Stairway(current.feature) && idleMs >= StairwayHoldMs) current.feature = PlaceFeature::Count;
	return current;
}

Change Compare(const Spot& before, const Spot& now) {
	Change change;
	const bool newMap = !before.valid || now.map != before.map;
	const bool nextBuilding = Separate(now.preset) && now.namedMap != before.namedMap;
	change.preset = now.preset != PresetPlace::None && (now.preset != before.preset || nextBuilding);
	const bool newInterior = newMap ? now.interior != None : now.interior != before.interior;
	change.interior = newInterior && now.interior != None;
	change.outside = before.valid && before.interior != None && now.interior == None;
	const bool stairway = before.valid && now.map == before.map && Stairway(before.feature) && Stairway(now.feature);
	change.feature = now.feature != PlaceFeature::Count && (now.feature != before.feature || change.interior) && !stairway;
	using Passages::Shape;
	change.node = (now.passage == Shape::Bend || now.passage == Shape::Junction) && now.passage != before.passage;
	if (now.passage == Shape::Corridor) {
		const bool fromNode = before.passage == Shape::Bend || before.passage == Shape::Junction;
		change.corridor = before.passage == Shape::None || (before.passage == Shape::Corridor && now.deadEnd && !before.deadEnd);
		change.branch = fromNode;
	}
	return change;
}
}
