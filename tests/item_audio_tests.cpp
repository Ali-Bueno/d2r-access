#include "item_wave.h"
#include "item_audio.h"
#include "loop_pace.h"
#include "item_spatial.h"
#include "test_support.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
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
	const auto load = [](const std::vector<uint8_t>& bytes) { return ItemWave::Parse(bytes); };
	auto bytes = Wave();
	Check(load(bytes) == std::vector<int16_t>{100, 200}, "mono PCM16 samples are preserved");
	for (size_t size = 0; size < bytes.size(); ++size) {
		auto truncated = bytes; truncated.resize(size);
		Check(load(truncated).empty(), "truncated audio is rejected without reading beyond the data");
	}
	for (size_t offset : {0u, 8u, 16u, 20u, 22u, 24u, 28u, 32u, 34u, 40u}) {
		auto invalid = bytes; invalid[offset] ^= 0x40;
		Check(load(invalid).empty(), "incompatible format or invalid chunk fails quiet");
	}
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
	const auto side = ItemSpatial::StereoMatrix(Voice{.right = 5.0f, .front = 5.0f});
	const auto power = [](const std::array<float, 2>& m) { return m[0] * m[0] + m[1] * m[1]; };
	const auto same = ItemSpatial::ScalePan(side, 1), none = ItemSpatial::ScalePan(side, 0);
	const auto half = ItemSpatial::ScalePan(side, 0.5), more = ItemSpatial::ScalePan(side, 2);
	Check(std::abs(same[0] - side[0]) < 1e-5f && std::abs(same[1] - side[1]) < 1e-5f, "pan scale 1 leaves X3DAudio's matrix");
	Check(std::abs(none[0] - none[1]) < 1e-5f, "pan scale 0 centres the sound");
	Check(half[1] - half[0] < side[1] - side[0] && half[1] > half[0], "a smaller pan scale keeps the side, nearer the middle");
	Check(more[1] - more[0] > side[1] - side[0], "a larger one moves it further out");
	Check(std::abs(power(none) - power(side)) < 1e-5f && std::abs(power(more) - power(side)) < 1e-5f, "the loudness is kept");
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
void DropPassTests() {
	const std::vector<int16_t> sound(10, 1000);
	const auto padded = ItemWave::Pass(sound, 16, 4);
	Check(padded.size() == 16 && padded[9] == 1000 && padded[10] == 0 && padded[15] == 0, "a short sound is padded with silence to its pass");
	const auto cut = ItemWave::Pass(sound, 6, 4);
	Check(cut.size() == 6 && cut[0] == 1000 && cut[5] == 0 && cut[4] > 0 && cut[4] < 1000, "a long sound is cut with a fade, never a click");
	Check(ItemAudio::DropPassSamples(ItemAudio::NeutralDropSpeedLevel) == ItemWave::SampleRate * ItemAudio::DropPassMs / 1000,
		"at 100 percent a drop sound repeats once a pass");
	Check(ItemAudio::DropPassSamples(0) > ItemAudio::DropPassSamples(static_cast<int>(ItemAudio::DropSpeedPercents.size()) - 1),
		"a faster speed repeats sooner");
}
}
void LoopPaceTests() {
	using GroundItems::Sound;
	Check(LoopPace::Of(Sound::WoodenDoor) == LoopPace::Group::Doors && LoopPace::Of(Sound::Scroll) == LoopPace::Group::SmallItems,
		"doors and small items pulse, each in its own group");
	Check(!LoopPace::Of(Sound::Gold) && !LoopPace::Of(Sound::Portal) && !LoopPace::Of(Sound::SlimeDoor),
		"steady loops have no beat to pace");
	Check(LoopPace::PassSamples(4800, LoopPace::NeutralLevel) == 0, "at 100 percent the recording plays as it is");
	Check(LoopPace::PassSamples(4800, PercentSteps::LevelOf(LoopPace::Percents, 200)) == 2400
		&& LoopPace::PassSamples(4800, PercentSteps::LevelOf(LoopPace::Percents, 50)) == 9600,
		"twice as fast halves the pass, half as fast doubles it");
}
int main(int argc, char** argv) {
	WaveTests(); MatrixTests(); NormalizationTests(); DropPassTests(); LoopPaceTests();
	Check(ItemAudio::IsOneShot(GroundItems::Sound::InsideUnexploredRoom), "room arrival uses a finite XAudio2 buffer");
	Check(ItemAudio::IsOneShot(GroundItems::Sound::Discovered), "the discovery cue plays once");
	Check(!ItemAudio::IsOneShot(GroundItems::Sound::UnexploredRoom)
		&& !ItemAudio::IsOneShot(GroundItems::Sound::ZoneTransition), "approach and zone cues loop");
	Check(argc == 2, "built plugin DLL supplied by CTest");
	// The plugin reads the same resources from its own module at run time.
	const HMODULE plugin = LoadLibraryExA(argv[1], nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	Check(plugin != nullptr, "built plugin DLL opens as a resource file");
	Check(ItemWave::LoadEmbedded(plugin, "loot", "missing.wav").empty(), "a missing embedded sound is handled");
	for (size_t sound = 0; sound < std::size(GroundItems::SoundFiles); ++sound) {
		const auto& file = GroundItems::SoundFiles[sound];
		auto samples = ItemWave::LoadEmbedded(plugin, file.folder, file.name);
		Check(!samples.empty(), std::string(file.name) + ": real user-provided cue is embedded as supported PCM");
		if (sound == static_cast<size_t>(GroundItems::Sound::Quest)) {
			Check(samples.size() == ItemWave::SampleRate * 700 / 1000, "quest recording repeats at the requested 700 ms without speeding up its pitch");
			Check(std::abs(int(samples.front())) < 100 && std::abs(int(samples.back())) < 100,
				"shortened quest loop has quiet edges instead of a hard cut");
		}
		if (sound == static_cast<size_t>(GroundItems::Sound::Stash))
			Check(samples.size() == ItemWave::SampleRate, "the stash recording is padded to repeat once a second");
		Check(ItemWave::Normalize(samples) > 0, "real loop contains a nonzero waveform");
		int peak = 0; for (auto sample : samples) peak = std::max(peak, std::abs(int(sample)));
		Check(std::abs(float(peak) / INT16_MAX - ItemWave::LoopPeak) < 0.0001f, "every real recording reaches the same peak with headroom");
		// At speech range, even a centered source must reach -34 dBFS per ear at
		// its loudest tenth of a second. This catches the reported silence: raw
		// gold/normal loops fell below -55 dBFS by either measure.
		if (sound < GroundItems::ItemSoundCount || GroundItems::IsDrop(static_cast<GroundItems::Sound>(sound))) {
			const double proximity = 1 - GroundItems::SpeakRadius / GroundItems::AudibleRadius;
			const auto matrix = ItemSpatial::StereoMatrix({.front = float(GroundItems::SpeakRadius)});
			Check(LoudestWindowRms(samples) * GroundItems::LoopVolume * proximity * proximity * matrix[0] >= 0.02,
				std::string(file.name) + ": real near-item mix meets the minimum signal level after distance and stereo panning");
		}
		Check(Rms(samples) > 0, "and carries signal across the whole file, not one spike");
	}
	FreeLibrary(plugin);
	Passed();
}
