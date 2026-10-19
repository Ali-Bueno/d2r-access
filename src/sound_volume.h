#pragma once

#include "percent_steps.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

// The volume of a family of the mod's sounds, a factor on top of the level its
// own config sets (a tester, 2026-10-13): radar and walls, enemies and NPCs,
// items and places, guidance. Header only, so the synthesizers stay untouched:
// the finished samples are scaled, saturating at full scale.
namespace D2RAccess::SoundVolume {
inline constexpr auto Percents = PercentSteps::Quarters<0, 200>();
inline constexpr int NeutralLevel = PercentSteps::LevelOf(Percents, 100);
static_assert(Percents[NeutralLevel] == 100);

constexpr double Factor(int level) {
	return Percents[static_cast<size_t>(std::clamp(level, 0, static_cast<int>(Percents.size()) - 1))] / 100.0;
}

inline void Scale(std::vector<int16_t>& pcm, double factor) {
	if (factor == 1) return;
	for (auto& sample : pcm) sample = static_cast<int16_t>(std::clamp(sample * factor, -32767.0, 32767.0));
}

// One family's setting, as the settings list trades it.
class Level {
public:
	int Get() const { return level_.load(); }
	void Set(int level) { level_ = std::clamp(level, 0, static_cast<int>(Percents.size()) - 1); }
	double Factor() const { return SoundVolume::Factor(level_.load()); }
private:
	std::atomic<int> level_{NeutralLevel};
};

// Radar and walls take the radar, the wall bump and the wall tones; enemies and NPCs every
// entity cue; items and places the ground loops; guidance the beacon. Each enemy class
// is a factor on top of the enemies' (a tester, 2026-10-15); low life is the heartbeat.
enum class Family { Radar, Enemies, Items, Guidance, OrdinaryEnemies, EliteEnemies, RangedEnemies, ShamanEnemies, BossEnemies, LowLife, Count };
inline Level& Of(Family family) {
	static std::array<Level, static_cast<size_t>(Family::Count)> levels;
	return levels[static_cast<size_t>(family)];
}
}
