#include "level_areas.h"

#include "level_exit_layout.h"
#include "room_graph.h"

#include <algorithm>
#include <utility>

namespace D2RAccess::LevelAreas {
namespace {

// Gap between two spans, measured between facing edges and negative where they
// overlap -- the same measure RoomGraph::Near takes, on one axis.
int Gap(int loA, int sizeA, int loB, int sizeB) {
	return loA < loB ? loB - (loA + sizeA) : loA - (loB + sizeB);
}

// Where the player leaves area A, in subtiles on one axis: the middle of the
// span the two areas share where they overlap, and otherwise A's own last
// subtile facing B. Room bounds are half-open, so the neighbour's edge would
// stand in no room of A and the automap would never chart the crossing.
double Leaving(int loA, int sizeA, int loB, int sizeB) {
	constexpr int Subtiles = LevelExitLayout::SubtilesPerTile;
	const int lo = std::max(loA, loB), hi = std::min(loA + sizeA, loB + sizeB);
	if (lo < hi) return static_cast<double>((lo + (hi - lo) / 2) * Subtiles);
	return static_cast<double>(loA + sizeA <= loB ? (loA + sizeA) * Subtiles - 1 : loA * Subtiles);
}

// The whole stretch Leaving takes the middle of: the shared span's first and
// last subtiles where the areas overlap, and otherwise A's last subtile alone.
std::pair<double, double> Stretch(int loA, int sizeA, int loB, int sizeB) {
	constexpr int Subtiles = LevelExitLayout::SubtilesPerTile;
	const int lo = std::max(loA, loB), hi = std::min(loA + sizeA, loB + sizeB);
	if (lo < hi) return {static_cast<double>(lo * Subtiles), static_cast<double>(hi * Subtiles - 1)};
	const double edge = Leaving(loA, sizeA, loB, sizeB);
	return {edge, edge};
}

}

std::vector<Crossing> Neighbours(const std::vector<NativeLevelAreas::Area>& areas, uint32_t currentLevel) {
	std::vector<Crossing> result;
	const auto here = std::find_if(areas.begin(), areas.end(),
		[&](const auto& area) { return area.level == currentLevel; });
	if (here == areas.end()) return result;
	for (const auto& area : areas) {
		if (area.level == currentLevel) continue;
		const int gapX = Gap(here->tileX, here->tileWidth, area.tileX, area.tileWidth);
		const int gapY = Gap(here->tileY, here->tileHeight, area.tileY, area.tileHeight);
		if (gapX >= RoomGraph::NearGapTiles || gapY >= RoomGraph::NearGapTiles) continue;
		// Corner to corner they share no edge to walk across (a tester, 2026-10-15:
		// the Monastery Gate named from the Black Marsh).
		if (gapX >= 0 && gapY >= 0) continue;
		Crossing crossing;
		crossing.level = area.level;
		crossing.position = {Leaving(here->tileX, here->tileWidth, area.tileX, area.tileWidth),
			Leaving(here->tileY, here->tileHeight, area.tileY, area.tileHeight)};
		const auto [firstX, lastX] = Stretch(here->tileX, here->tileWidth, area.tileX, area.tileWidth);
		const auto [firstY, lastY] = Stretch(here->tileY, here->tileHeight, area.tileY, area.tileHeight);
		crossing.first = {firstX, firstY};
		crossing.last = {lastX, lastY};
		crossing.gapTiles = std::max({gapX, gapY, 0});
		result.push_back(crossing);
	}
	return result;
}

}
