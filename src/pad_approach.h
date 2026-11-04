#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// The pad takes an NPC only within 4 subtiles, where a click takes any on screen. Raising
// that reach is safe only while the press has nothing else to take: the game prefers an
// NPC to a monster out of reach, and a far NPC outscores a stash at hand. docs/input-commands.md
namespace D2RAccess::PadApproach {
// What the game's score weighs for an NPC: how far off it counts, in subtiles, and how much
// distance counts against aim. The game has 4 and 1, the nearest wins wherever the stick
// points. On, 35 is its default for a category, the on-screen test limiting it, and 0.35
// its own balance for aiming at monsters (MonsterMiddleGround).
struct NpcTargeting {
	float reach = 0, weight = 0;
	bool operator==(const NpcTargeting&) const = default;
};
constexpr NpcTargeting ByAim{35.f, .35f};

// The game's per-frame target slots, one per category.
constexpr size_t Slots = 12;
constexpr int32_t EmptySlot = -1;
constexpr size_t OtherTargets[] = {1, 2, 3, 4, 9, 10}; // monster, object, item, room tile, player corpse, hostile

inline bool NothingElseAtHand(const std::array<int32_t, Slots>& ids) noexcept {
	for (const size_t slot : OtherTargets)
		if (ids[slot] != EmptySlot) return false;
	return true;
}

struct State {
	NpcTargeting game;
	bool raised = false;
};

// What to write to the game's fields, if anything. They are the game's: it may load its
// values again, and values that are not ours are never overwritten on the way back.
inline std::optional<NpcTargeting> Next(State& state, bool wanted, NpcTargeting current) noexcept {
	if (wanted) {
		if (current == ByAim) return std::nullopt;
		state = {current, true};
		return ByAim;
	}
	if (!state.raised) return std::nullopt;
	state.raised = false;
	if (current != ByAim) return std::nullopt;
	return state.game;
}
}
