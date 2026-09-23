#pragma once

#include <D2RLPlugin/api.h>
#include <cstdint>
#include <optional>

namespace D2RAccess::NativeStatus {

bool Initialize(const D2RL::PluginContext* context);
// No reading when the player is absent, dead, or the verified native path fails.
std::optional<bool> PlayerPoisoned() noexcept;

struct Mercenary {
	uint32_t id = 0;
	int currentLife = 0, maximumLife = 0;
};
// The unit bound to the game's HirelingInventoryPanel; absent is unknown.
std::optional<Mercenary> MercenaryLife() noexcept;

}
