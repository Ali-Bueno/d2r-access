#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// When a weapon swap is announced and what it says. See docs/weapon-swap.md.
namespace D2RAccess::WeaponSwap {
// Body locations of the weapons in hand (equipmentSlots' right and left weapon).
inline constexpr int RightHand = 4, LeftHand = 5;
// The server may move the items and flip the set in separate replies; a set
// whose hands have not changed by then holds no weapons to move.
inline constexpr uint64_t ItemMoveGraceMs = 500;

// Runtime ids in each hand, 0 when empty.
struct Hands {
	uint32_t right = 0, left = 0;
	bool operator==(const Hands&) const = default;
};

class Tracker {
public:
	// The set (1 or 2) to announce now, if any. The first reading is the
	// baseline, so entering a game on set II says nothing.
	std::optional<int> Update(uint64_t now, std::optional<bool> secondSet, const Hands& hands);
	void Reset();

private:
	struct Pending { int set = 1; Hands from; uint64_t due = 0; };
	std::optional<bool> second_;
	Hands hands_;
	std::optional<Pending> pending_;
};

// "Arco largo, Carcaj" or "sin armas": the weapons now in hand. The set number
// leads when its switch is on, as it is by default (the user, 2026-10-12).
std::string Phrase(const std::vector<std::string>& weapons, std::optional<int> set = std::nullopt);
}
