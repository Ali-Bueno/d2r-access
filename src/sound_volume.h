#pragma once

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
inline constexpr std::array Percents{0, 25, 50, 75, 100, 125, 150, 200};
inline constexpr int NeutralLevel = 4;
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
// entity cue; items and places the ground loops; guidance the beacon.
enum class Family { Radar, Enemies, Items, Guidance, Count };
inline Level& Of(Family family) {
	static std::array<Level, static_cast<size_t>(Family::Count)> levels;
	return levels[static_cast<size_t>(family)];
}
}
