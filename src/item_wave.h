#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace D2RAccess::ItemWave {
inline constexpr uint32_t SampleRate = 48000;
// Normalize each recording before spatial gain; leave 10% sample headroom.
inline constexpr float LoopPeak = 0.9f;
// Build converts OGG assets to mono PCM16; malformed or incompatible files fail quiet.
std::vector<int16_t> Load(const std::filesystem::path& path);
// Returns the applied multiplier, or zero for an empty/silent recording.
float Normalize(std::vector<int16_t>& samples);
}
