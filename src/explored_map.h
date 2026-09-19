#pragma once

#include "radar.h"

#include <array>
#include <cstdint>
#include <unordered_map>

// Where the player has already been on this level.
//
// D2R streams collision rooms around the player, so the world snapshot cannot
// say what has been explored: a room left behind unloads and reads as unknown
// again, exactly like one never entered. This keeps the mod's own memory
// instead — every poll paints a disc around the player — so "unexplored" means
// "I have not walked there", the sense the player asked for.
//
// Cells are world collision subtiles, stored as a sparse map of 32x32 bitmap
// chunks: a whole level is a few hundred chunks, and nothing is allocated for
// the parts of the coordinate space a level never uses.
namespace D2RAccess::Explored {
struct Settings {
	// Radius in subtiles painted around the player. Walking a corridor leaves a
	// band twice this wide behind it, so side rooms stay unexplored while the
	// corridor itself does not come back as a suggestion.
	int markRadius = 12;
};

class Map {
public:
	// A different level or game starts an empty memory; the same one keeps it.
	void Follow(uint64_t session, int level);
	void Reset();
	void Mark(Radar::Position player, const Settings& settings = {});
	bool Seen(int x, int y) const;
	size_t Chunks() const { return chunks_.size(); }
	int Level() const { return level_; }

private:
	static constexpr int ChunkSide = 32;
	static constexpr size_t ChunkBytes = ChunkSide * ChunkSide / 8;
	using Chunk = std::array<uint8_t, ChunkBytes>;
	std::unordered_map<uint32_t, Chunk> chunks_;
	uint64_t session_ = 0;
	int level_ = 0;
	bool following_ = false;
};
}
