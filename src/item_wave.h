#pragma once
#include <windows.h>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace D2RAccess::ItemWave {
inline constexpr uint32_t SampleRate = 48000;
// Normalize each recording before spatial gain; leave 10% sample headroom.
inline constexpr float LoopPeak = 0.9f;
// Build converts OGG assets to mono PCM16; malformed or incompatible data fails quiet.
std::vector<int16_t> Parse(std::span<const uint8_t> bytes);
// The build embeds each WAV in the plugin DLL as RCDATA (cmake/GroundItems.cmake).
std::vector<int16_t> LoadEmbedded(HMODULE module, std::string_view folder, std::string_view file);
// Returns the applied multiplier, or zero for an empty/silent recording.
float Normalize(std::vector<int16_t>& samples);
}
