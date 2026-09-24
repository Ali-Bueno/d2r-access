#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace D2RAccess::CairnStones {
// Installed objects.txt StoneAlpha..StoneLambda; the scroll packet encodes class - 17.
inline constexpr uint32_t FirstClass = 17;
inline constexpr size_t Count = 5;
using Order = std::array<uint16_t, Count>;

constexpr bool Valid(const Order& order) {
	std::array<bool, Count> seen{};
	for (auto stone : order) {
		if (stone >= Count || seen[stone]) return false;
		seen[stone] = true;
	}
	return true;
}

constexpr std::optional<size_t> Index(uint32_t classId) {
	if (classId < FirstClass || classId >= FirstClass + Count) return {};
	return classId - FirstClass;
}

}
