#pragma once

#include <D2RLPlugin/api.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace D2RAccess::NativeStatus {

bool Initialize(const D2RL::PluginContext* context);
// Which of `states` (states.txt rows) are on the local player, as indexes into
// `states`. No reading when the player is absent, dead, or the verified native path fails.
std::optional<std::vector<size_t>> PlayerStates(std::span<const int> states);

// The local player's gait and stamina; absent when the player is absent,
// dead, or the verified path fails. The gait is known only while moving.
struct Movement {
	bool walking = false, running = false;
	int stamina = -1, maxStamina = -1; // the game's own fixed-point units
};
std::optional<Movement> PlayerMovement() noexcept;

// The local player's `stats` (ItemStatCost.txt rows, at most MaxStats) and whether it is swinging
// (attacking, kicking, throwing); absent as PlayerMovement is.
inline constexpr size_t MaxStats = 8;
struct Stats {
	std::vector<int> values;
	bool swinging = false;
};
std::optional<Stats> PlayerStats(std::span<const int> stats);

// The local player's life and mana in the game's own fixed-point units, a dead player's too.
struct Orbs { int life = 0, maxLife = 0, mana = 0, maxMana = 0; };
std::optional<Orbs> PlayerOrbs() noexcept;

// The local player's experience within its level, as the HUD's bar fills.
struct Experience {
	int level = 0, percent = -1;
	uint64_t total = 0, levelStart = 0, nextLevel = 0;
	bool maximum = false; // at the level cap there is nothing to reach
};
std::optional<Experience> PlayerExperience() noexcept;
// The local player's points not yet spent.
struct Unspent { int attributes = 0, skills = 0; };
std::optional<Unspent> PlayerUnspent() noexcept;

struct Mercenary {
	uint32_t id = 0;
	int lifePercent = -1;
	int level = -1;
	int experiencePercent = -1; // within its level, -1 unknown
};
// The unit bound to the game's HirelingInventoryPanel; absent is unknown.
std::optional<Mercenary> MercenaryLife() noexcept;

// A loaded monster's life as its health plate shows it, 1-100, or 0 once dead;
// absent when the unit is not loaded or the verified path fails.
std::optional<int> MonsterLife(uint32_t id) noexcept;

}
