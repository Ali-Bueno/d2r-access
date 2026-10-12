#pragma once

#include <D2RLPlugin/api.h>
#include <cstdint>
#include <optional>

namespace D2RAccess::NativeStatus {

bool Initialize(const D2RL::PluginContext* context);
// No reading when the player is absent, dead, or the verified native path fails.
std::optional<bool> PlayerPoisoned() noexcept;

// The local player's gait and stamina; absent when the player is absent,
// dead, or the verified path fails. The gait is known only while moving.
struct Movement {
	bool walking = false, running = false;
	int stamina = -1, maxStamina = -1; // the game's own fixed-point units
};
std::optional<Movement> PlayerMovement() noexcept;

struct Mercenary {
	uint32_t id = 0;
	int lifePercent = -1;
	int level = -1;
};
// The unit bound to the game's HirelingInventoryPanel; absent is unknown.
std::optional<Mercenary> MercenaryLife() noexcept;

// A loaded monster's life as its health plate shows it, 1-100, or 0 once dead;
// absent when the unit is not loaded or the verified path fails.
std::optional<int> MonsterLife(uint32_t id) noexcept;

}
