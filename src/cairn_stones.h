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
enum class State { Unknown, Unactivated, Activated };
using States = std::array<State, Count>;

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

constexpr std::optional<size_t> Rank(const Order& order, size_t stone) {
	if (!Valid(order) || stone >= Count) return {};
	for (size_t i = 0; i < Count; ++i) if (order[i] == stone) return i + 1;
	return {};
}

// An unloaded earlier stone cannot be presumed activated; inconsistent progress stays unknown.
constexpr std::optional<size_t> Next(const Order& order, const States& states) {
	if (!Valid(order)) return {};
	std::optional<size_t> next;
	bool gap = false;
	for (auto stone : order) {
		if (states[stone] == State::Activated) {
			if (gap) return {};
			continue;
		}
		if (!gap && states[stone] == State::Unactivated) next = stone;
		gap = true;
	}
	return next;
}
}
