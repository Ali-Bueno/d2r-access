#pragma once

#include <cstdint>
#include <functional>
#include <optional>

// Whether the game's InitFn of an NPC marker object (Generated::NpcMarkers) spawns its
// NPC, told from the quest record rows it tests. Pure logic. docs/entity-scan.md#npcs-at-marker-objects
namespace D2RAccess::NpcSpawnRules {
// A quest record row's flag word, nothing when it cannot be read.
using QuestFlags = std::function<std::optional<uint16_t>(int row)>;

// Nothing for an InitFn whose test the flags cannot tell, or when a row is unreadable.
std::optional<bool> Spawns(int initFn, const QuestFlags& flags);
}
