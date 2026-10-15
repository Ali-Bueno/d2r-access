#pragma once

#include <array>
#include <cstddef>

// Every percent setting of the mod moves in quarters (a tester, 2026-10-14).
namespace D2RAccess::PercentSteps {
inline constexpr int Step = 25;

// Each quarter from `From` to `To` percent, both included.
template<int From, int To> constexpr auto Quarters() {
	static_assert(From % Step == 0 && To % Step == 0 && From < To);
	std::array<int, static_cast<size_t>((To - From) / Step + 1)> percents{};
	for (size_t i = 0; i < percents.size(); ++i) percents[i] = From + static_cast<int>(i) * Step;
	return percents;
}

// The level of `percent` among `percents`, -1 when it is not one of them.
template<size_t N> constexpr int LevelOf(const std::array<int, N>& percents, int percent) {
	for (size_t i = 0; i < N; ++i) if (percents[i] == percent) return static_cast<int>(i);
	return -1;
}
}
