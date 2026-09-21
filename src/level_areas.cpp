#include "level_areas.h"

#include "level_exit_layout.h"
#include "room_graph.h"

#include <algorithm>

namespace D2RAccess::LevelAreas {
namespace {

// Gap between two spans, measured between facing edges and negative where they
// overlap -- the same measure RoomGraph::Near takes, on one axis.
int Gap(int loA, int sizeA, int loB, int sizeB) {
	return loA < loB ? loB - (loA + sizeA) : loA - (loB + sizeB);
}

// Where the player crosses, on one axis: the middle of the span the two areas
// share where they overlap, and otherwise the neighbour's facing edge.
int Meeting(int loA, int sizeA, int loB, int sizeB) {
	const int lo = std::max(loA, loB), hi = std::min(loA + sizeA, loB + sizeB);
	if (lo < hi) return lo + (hi - lo) / 2;
	return loA + sizeA <= loB ? loB : loB + sizeB;
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
		const int x = Meeting(here->tileX, here->tileWidth, area.tileX, area.tileWidth);
		const int y = Meeting(here->tileY, here->tileHeight, area.tileY, area.tileHeight);
		Crossing crossing;
		crossing.level = area.level;
		crossing.position = {static_cast<double>(x) * LevelExitLayout::SubtilesPerTile,
			static_cast<double>(y) * LevelExitLayout::SubtilesPerTile};
		crossing.gapTiles = std::max({gapX, gapY, 0});
		result.push_back(crossing);
	}
	return result;
}

}
