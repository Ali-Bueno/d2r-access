#include "explored_map.h"

#include <cmath>

namespace D2RAccess::Explored {
namespace {
// Native positions are unsigned 16-bit cells (native_world.cpp validates them),
// so a key packs the two chunk coordinates without loss.
constexpr int MaxCoordinate = 0xFFFF;
uint32_t Key(int chunkX, int chunkY) { return static_cast<uint32_t>(chunkX) << 16 | static_cast<uint32_t>(chunkY); }
}

void Map::Follow(uint64_t session, int level) {
	if (following_ && session == session_ && level == level_) return;
	Reset();
	session_ = session;
	level_ = level;
	following_ = true;
}

void Map::Reset() {
	chunks_.clear();
	session_ = 0;
	level_ = 0;
	following_ = false;
}

void Map::Mark(Radar::Position player, const Settings& settings) {
	if (!std::isfinite(player.x) || !std::isfinite(player.y) || settings.markRadius < 0) return;
	const int centreX = static_cast<int>(std::lround(player.x)), centreY = static_cast<int>(std::lround(player.y));
	if (centreX < 0 || centreY < 0 || centreX > MaxCoordinate || centreY > MaxCoordinate) return;
	const int radius = settings.markRadius;
	for (int dy = -radius; dy <= radius; ++dy) {
		// A disc, not a square: the corners of a square would swallow rooms
		// diagonally adjacent to a corridor the player only walked past.
		const int span = static_cast<int>(std::sqrt(static_cast<double>(radius) * radius - static_cast<double>(dy) * dy));
		const int y = centreY + dy;
		if (y < 0 || y > MaxCoordinate) continue;
		for (int dx = -span; dx <= span; ++dx) {
			const int x = centreX + dx;
			if (x < 0 || x > MaxCoordinate) continue;
			auto& chunk = chunks_[Key(x / ChunkSide, y / ChunkSide)];
			const size_t bit = static_cast<size_t>(y % ChunkSide) * ChunkSide + static_cast<size_t>(x % ChunkSide);
			chunk[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
		}
	}
}

bool Map::Seen(int x, int y) const {
	if (x < 0 || y < 0 || x > MaxCoordinate || y > MaxCoordinate) return false;
	const auto chunk = chunks_.find(Key(x / ChunkSide, y / ChunkSide));
	if (chunk == chunks_.end()) return false;
	const size_t bit = static_cast<size_t>(y % ChunkSide) * ChunkSide + static_cast<size_t>(x % ChunkSide);
	return (chunk->second[bit / 8] & (1u << (bit % 8))) != 0;
}
}
