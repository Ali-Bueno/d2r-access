#include "entity_cue_audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace D2RAccess::EntityCueAudio {
namespace {
// Listening feedback, 2026-09-22: one soft, dry strike instead of melodic notes.
// Short inharmonic resonances and filtered texture keep the categories distinct.
constexpr unsigned CueMs = 84;
constexpr double RearPitch = 0.80; // a major-third drop makes rear audible without changing the cue identity
constexpr double AttackSeconds = 0.0035, ReleaseSeconds = 0.012;
constexpr double Level = 0.32, PcmPeak = 32767, TextureCutoffHz = 1800;
constexpr double BodyLevel = 0.75, UpperLevel = 0.30, TopLevel = 0.14;
constexpr double UpperRatio = 1.57, TopRatio = 2.31;
constexpr size_t CueFrames = CueMs * SampleRate / 1000;

struct Timbre { double hz, decaySeconds, texture; };
Timbre Voice(Sound sound) {
	switch (sound) {
	case Sound::Npc: return {850, 0.013, 0.38}; // light wooden tap
	case Sound::Resurrector: return {430, 0.018, 0.60}; // grainier knock
	case Sound::Ranged: return {550, 0.012, 0.50}; // tight, dry tick
	default: return {330, 0.021, 0.28}; // important enemy: low, rounded thud
	}
}

double SafeAxis(double value, double fallback) {
	return std::isfinite(value) ? std::clamp(value, -1.0, 1.0) : fallback;
}

double Noise(size_t frame) {
	uint32_t hash = static_cast<uint32_t>(frame) * 1664525u + 1013904223u;
	hash ^= hash >> 16;
	return static_cast<double>(hash & 0xffffu) / 32767.5 - 1.0;
}

double Envelope(double seconds, double duration) {
	return std::clamp(seconds / AttackSeconds, 0.0, 1.0)
		* std::clamp((duration - seconds - 1.0 / SampleRate) / ReleaseSeconds, 0.0, 1.0);
}
}

std::vector<int16_t> Synthesize(Sound sound, Placement placement, double gain) {
	const double safeGain = std::isfinite(gain) ? std::clamp(gain, 0.0, 1.0) : 0;
	const double pan = SafeAxis(placement.right, 0);
	const double front = SafeAxis(placement.front, 1);
	const double pitch = 1 + (RearPitch - 1) * std::clamp(-front, 0.0, 1.0);
	const double left = std::sqrt((1 - pan) / 2);
	const double right = std::sqrt((1 + pan) / 2);
	const auto timbre = Voice(sound);
	const double textureBlend = 1 - std::exp(-2 * std::numbers::pi * TextureCutoffHz * pitch / SampleRate);
	double texture = 0;
	std::vector<int16_t> pcm(CueFrames * Channels);

	for (size_t frame = 0; frame < CueFrames; ++frame) {
		const double seconds = static_cast<double>(frame) / SampleRate;
		texture += textureBlend * (Noise(frame) - texture);
		const double phase = 2 * std::numbers::pi * timbre.hz * pitch * seconds;
		const double body = BodyLevel * std::sin(phase) + UpperLevel * std::sin(UpperRatio * phase)
			+ TopLevel * std::sin(TopRatio * phase);
		const double envelope = Envelope(seconds, static_cast<double>(CueMs) / 1000)
			* std::exp(-seconds / timbre.decaySeconds);
		const double sample = Level * envelope * (body + timbre.texture * texture);
		const double scaled = sample * safeGain * PcmPeak;
		pcm[frame * Channels] = static_cast<int16_t>(std::clamp(scaled * left, -PcmPeak, PcmPeak));
		pcm[frame * Channels + 1] = static_cast<int16_t>(std::clamp(scaled * right, -PcmPeak, PcmPeak));
	}
	return pcm;
}
}
