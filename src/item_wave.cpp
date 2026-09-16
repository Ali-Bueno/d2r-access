#include "item_wave.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>

namespace D2RAccess::ItemWave {
namespace {
// Each loop is short; cap per-file allocation at one minute of mono PCM16.
constexpr size_t MaxFileBytes = SampleRate * sizeof(int16_t) * 60;
uint32_t U32(std::span<const uint8_t> bytes, size_t offset) {
	return bytes[offset] | (uint32_t(bytes[offset + 1]) << 8) | (uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
}
uint16_t U16(std::span<const uint8_t> bytes, size_t offset) { return bytes[offset] | (uint16_t(bytes[offset + 1]) << 8); }
bool Tag(std::span<const uint8_t> bytes, size_t offset, const char* tag) { return std::memcmp(bytes.data() + offset, tag, 4) == 0; }
}
float Normalize(std::vector<int16_t>& samples) {
	int peak = 0;
	for (const auto sample : samples) peak = std::max(peak, std::abs(static_cast<int>(sample)));
	if (!peak) return 0;
	const float scale = LoopPeak * std::numeric_limits<int16_t>::max() / peak;
	for (auto& sample : samples) sample = static_cast<int16_t>(std::lround(sample * scale));
	return scale;
}
std::vector<int16_t> Load(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) return {};
	const auto length = file.tellg();
	if (length < 12 || length > static_cast<std::streamoff>(MaxFileBytes)) return {};
	std::vector<uint8_t> bytes(static_cast<size_t>(length));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(bytes.data()), length) || !Tag(bytes, 0, "RIFF") || !Tag(bytes, 8, "WAVE")) return {};
	const size_t end = static_cast<size_t>(U32(bytes, 4)) + 8;
	if (end != bytes.size()) return {};
	bool format = false;
	size_t data = 0, dataSize = 0;
	for (size_t at = 12; at < end;) {
		if (end - at < 8) return {};
		const size_t size = U32(bytes, at + 4), start = at + 8;
		if (size > end - start || size + (size & 1) > end - start) return {};
		if (Tag(bytes, at, "fmt ")) {
			if (format || size < 16 || U16(bytes, start) != 1 || U16(bytes, start + 2) != 1
				|| U32(bytes, start + 4) != SampleRate || U32(bytes, start + 8) != SampleRate * sizeof(int16_t)
				|| U16(bytes, start + 12) != sizeof(int16_t) || U16(bytes, start + 14) != 16) return {};
			format = true;
		} else if (Tag(bytes, at, "data")) {
			if (data || !size || size % sizeof(int16_t)) return {};
			data = start; dataSize = size;
		}
		at = start + size + (size & 1);
	}
	if (!format || !data) return {};
	std::vector<int16_t> result(dataSize / sizeof(int16_t));
	std::memcpy(result.data(), bytes.data() + data, dataSize);
	return result;
}
}
