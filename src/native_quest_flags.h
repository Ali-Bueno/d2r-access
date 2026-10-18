#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <optional>

// The local player's quest flags as the client keeps them: one 16-bit word per
// quest record row, for the difficulty being played. Read-only. docs/quests.md
namespace D2RAccess::NativeQuestFlags {
bool Initialize(const D2RL::PluginContext* context);
// The flag word of quest record row `quest`, or nothing outside a game, on another
// game build, or before the game's bit masks prove the word's bit order. Any thread.
std::optional<uint16_t> Read(int quest);
}
