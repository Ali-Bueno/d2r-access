#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

// Whether the game's InitFn of an NPC marker object (Generated::NpcMarkers) spawns its
// NPC, told from the quest record rows it tests. Pure logic. docs/entity-scan.md#npcs-at-marker-objects
namespace D2RAccess::NpcSpawnRules {
// A quest record row's flag word, nothing when it cannot be read.
using QuestFlags = std::function<std::optional<uint16_t>(int row)>;

// The levels the player has been in during one game, kept by its session.
class GameLevels {
public:
	void Note(uint64_t session, int level);
	bool Entered(uint64_t session, int level) const;
private:
	uint64_t session_ = 0;
	std::vector<int> levels_;
};

// What the game itself keeps for the current game, beyond the save's quest record.
struct ThisGame {
	std::function<bool(int level)> entered; // the player has been in `level` this game
	bool shared = false; // other players are in it: its quest data may be another's
};

// Nothing for an InitFn whose test the flags cannot tell, or when a row is unreadable.
std::optional<bool> Spawns(int initFn, const QuestFlags& flags, const ThisGame& game = {});
}
