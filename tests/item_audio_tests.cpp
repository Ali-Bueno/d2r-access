#include "item_wave.h"
#include "item_audio.h"
#include "item_spatial.h"
#include "test_support.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

using namespace D2RAccess;
namespace {
std::vector<uint8_t> Wave() {
	std::vector<uint8_t> bytes(48);
	const auto tag = [&](size_t at, const char* text) { std::memcpy(bytes.data() + at, text, 4); };
	const auto word = [&](size_t at, uint16_t value) { std::memcpy(bytes.data() + at, &value, 2); };
	const auto dword = [&](size_t at, uint32_t value) { std::memcpy(bytes.data() + at, &value, 4); };
	tag(0, "RIFF"); dword(4, 40); tag(8, "WAVE"); tag(12, "fmt "); dword(16, 16);
	word(20, 1); word(22, 1); dword(24, ItemWave::SampleRate); dword(28, ItemWave::SampleRate * 2);
	word(32, 2); word(34, 16); tag(36, "data"); dword(40, 4); word(44, 100); word(46, 200);
	return bytes;
}
void WaveTests() {
	const auto path = std::filesystem::temp_directory_path() / ("d2r-item-wave-" + std::to_string(GetCurrentProcessId()) + ".wav");
	const auto load = [&](const std::vector<uint8_t>& bytes) {
		{ std::ofstream file(path, std::ios::binary); file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()); }
		return ItemWave::Load(path);
	};
	auto bytes = Wave();
	Check(load(bytes) == std::vector<int16_t>{100, 200}, "mono PCM16 samples are preserved");
	for (size_t size = 0; size < bytes.size(); ++size) {
		auto truncated = bytes; truncated.resize(size);
		Check(load(truncated).empty(), "truncated audio is rejected without reading beyond the file");
	}
	for (size_t offset : {0u, 8u, 16u, 20u, 22u, 24u, 28u, 32u, 34u, 40u}) {
		auto invalid = bytes; invalid[offset] ^= 0x40;
		Check(load(invalid).empty(), "incompatible format or invalid chunk fails quiet");
	}
	std::filesystem::remove(path);
	Check(ItemWave::Load(path).empty(), "missing audio is handled");
}
void MatrixTests() {
	using GroundItems::Voice;
	for (float distance : {1.0f, 5.0f, 20.0f}) {
		const auto left = ItemSpatial::StereoMatrix(Voice{.right = -distance});
		const auto right = ItemSpatial::StereoMatrix(Voice{.right = distance});
		const auto front = ItemSpatial::StereoMatrix(Voice{.front = distance});
		const auto rear = ItemSpatial::StereoMatrix(Voice{.front = -distance});
		Check(left[0] > left[1] && right[1] > right[0], "actual X3DAudio matrix preserves left/right");
		Check(front[0] > 0 && std::abs(front[0] - front[1]) < 0.0001f, "front is centered");
		Check(rear[0] > 0 && std::abs(rear[0] - rear[1]) < 0.0001f, "rear is centered, leaving pitch to disambiguate it");
	}
	const auto center = ItemSpatial::StereoMatrix({});
	Check(std::isfinite(center[0]) && std::isfinite(center[1]) && center[0] > 0 && center[1] > 0, "standing directly on a source gives finite stereo output");
}
double Rms(const std::vector<int16_t>& samples) {
	double energy = 0;
	for (auto sample : samples) energy += double(sample) * sample;
	return std::sqrt(energy / samples.size()) / std::numeric_limits<int16_t>::max();
}
// The level of the loudest tenth of a second. A steady bed and a struck cue
// carry their signal differently — the chest cue is one hit and a decay, so its
// average over the whole file understates what the ear gets — and this measures
// both by what the listener actually hears at the loudest moment.
double LoudestWindowRms(const std::vector<int16_t>& samples) {
	const size_t window = std::min<size_t>(ItemWave::SampleRate / 10, samples.size());
	if (!window) return 0;
	double best = 0;
	for (size_t start = 0; start + window <= samples.size(); start += window) {
		double energy = 0;
		for (size_t i = start; i < start + window; ++i) energy += double(samples[i]) * samples[i];
		best = std::max(best, std::sqrt(energy / window) / std::numeric_limits<int16_t>::max());
	}
	return best;
}
void NormalizationTests() {
	std::vector<int16_t> empty, silent(128, 0);
	Check(ItemWave::Normalize(empty) == 0 && ItemWave::Normalize(silent) == 0, "silent recordings are rejected without dividing by zero");
	std::vector<int16_t> quiet{0, 50, 100, -100, -50};
	Check(ItemWave::Normalize(quiet) > 1, "quiet recordings are amplified before distance attenuation");
	Check(quiet[0] == 0 && quiet[2] == -quiet[3] && std::abs(quiet[1] * 2 - quiet[2]) <= 1, "normalization preserves silence, polarity and waveform proportions");
	std::vector<int16_t> loud{INT16_MIN, INT16_MAX};
	Check(ItemWave::Normalize(loud) < 1 && loud[0] < 0 && loud[1] > 0, "full scale negative samples normalize without overflow");
	Check(std::abs(loud[0]) < INT16_MAX && loud[1] < INT16_MAX, "normalized samples leave headroom without clipping");
}
}
int main(int argc, char** argv) {
	WaveTests(); MatrixTests(); NormalizationTests();
	Check(ItemAudio::IsOneShot(GroundItems::Sound::InsideUnexploredRoom), "room arrival uses a finite XAudio2 buffer");
	Check(!ItemAudio::IsOneShot(GroundItems::Sound::UnexploredRoom)
		&& !ItemAudio::IsOneShot(GroundItems::Sound::ZoneTransition), "approach and zone cues loop");
	Check(argc == 3, "converted sample directories supplied by CTest");
	for (size_t sound = 0; sound < std::size(GroundItems::SoundFiles); ++sound) {
		const auto* name = GroundItems::SoundFiles[sound];
		const auto* folder = sound < GroundItems::ItemSoundCount ? argv[1] : argv[2];
		auto samples = ItemWave::Load(std::filesystem::path(folder) / name);
		Check(!samples.empty(), "real user-provided cue decodes into supported PCM");
		Check(ItemWave::Normalize(samples) > 0, "real loop contains a nonzero waveform");
		int peak = 0; for (auto sample : samples) peak = std::max(peak, std::abs(int(sample)));
		Check(std::abs(float(peak) / INT16_MAX - ItemWave::LoopPeak) < 0.0001f, "every real recording reaches the same peak with headroom");
		// At speech range, even a centered source must reach -34 dBFS per ear at
		// its loudest tenth of a second. This catches the reported silence: raw
		// gold/normal loops fell below -55 dBFS by either measure.
		if (sound < GroundItems::ItemSoundCount) {
			const double proximity = 1 - GroundItems::SpeakRadius / GroundItems::AudibleRadius;
			const auto matrix = ItemSpatial::StereoMatrix({.front = float(GroundItems::SpeakRadius)});
			Check(LoudestWindowRms(samples) * GroundItems::LoopVolume * proximity * proximity * matrix[0] >= 0.02,
				"real near-item mix meets the minimum signal level after distance and stereo panning");
		}
		Check(Rms(samples) > 0, "and carries signal across the whole file, not one spike");
	}
	Passed();
}
