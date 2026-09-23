#include "entity_cue_audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace D2RAccess::EntityCueAudio {
namespace {
// Listening feedback, 2026-09-22: a voiced metallic swoop separates enemies
// from the radar's filtered-noise breath. Both fit the faster 100 ms spacing.
constexpr unsigned NpcCueMs = 84, WhooshMs = 90;
constexpr double RearPitch = 0.80; // a major-third drop makes rear audible without changing the cue identity
constexpr double AttackSeconds = 0.0035, ReleaseSeconds = 0.012;
constexpr double Level = 0.32, PcmPeak = 32767, TextureCutoffHz = 1800;
constexpr double BodyLevel = 0.75, UpperLevel = 0.30, TopLevel = 0.14;
constexpr double UpperRatio = 1.57, TopRatio = 2.31;
constexpr double NpcHz = 850, NpcDecaySeconds = 0.013, NpcTexture = 0.38;
constexpr double WhooshLevel = 0.29, WhooshMetalDepth = 0.65;
constexpr double WhooshBody = 0.88, WhooshAir = 0.12, WhooshAirCutoffHz = 1500;

struct WhooshTimbre { double startHz, endHz, shimmerRatio; };
WhooshTimbre Voice(Sound sound) {
	switch (sound) {
	case Sound::Resurrector: return {680, 160, 1.83};
	case Sound::Ranged: return {880, 240, 2.19};
	default: return {480, 110, 1.61}; // important enemies have the deepest sweep
	}
}

struct WhooshState {
	double air = 0, phase = 0;
	uint32_t noise = 0x6D2B79F5u; // fixed nonzero seed keeps synthesized texture reproducible
};
double Whoosh(WhooshState& state, WhooshTimbre timbre, double progress, double pitch) {
	state.noise ^= state.noise << 13;
	state.noise ^= state.noise >> 17;
	state.noise ^= state.noise << 5;
	const double air = static_cast<double>(state.noise) / UINT32_MAX * 2 - 1;
	const double remaining = 1 - progress;
	const double hz = (timbre.endHz + (timbre.startHz - timbre.endHz) * remaining * remaining) * pitch;
	const double angularStep = 2 * std::numbers::pi * hz / SampleRate;
	const double airBlend = 1 - std::exp(-2 * std::numbers::pi * WhooshAirCutoffHz * pitch / SampleRate);
	state.air += airBlend * (air - state.air);
	state.phase += angularStep;
	// A falling inharmonic resonance gives a low "vwoop", with only a little air.
	const double body = std::sin(state.phase + WhooshMetalDepth * std::sin(state.phase * timbre.shimmerRatio));
	const double swell = std::sin(std::numbers::pi * progress);
	return WhooshLevel * swell * swell * (WhooshBody * body + WhooshAir * state.air);
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
	const bool npc = sound == Sound::Npc;
	const size_t frames = (npc ? NpcCueMs : WhooshMs) * SampleRate / 1000;
	const double textureBlend = 1 - std::exp(-2 * std::numbers::pi * TextureCutoffHz * pitch / SampleRate);
	double texture = 0;
	WhooshState whoosh;
	std::vector<int16_t> pcm(frames * Channels);

	for (size_t frame = 0; frame < frames; ++frame) {
		const double seconds = static_cast<double>(frame) / SampleRate;
		double sample = 0;
		if (npc) {
			texture += textureBlend * (Noise(frame) - texture);
			const double phase = 2 * std::numbers::pi * NpcHz * pitch * seconds;
			const double body = BodyLevel * std::sin(phase) + UpperLevel * std::sin(UpperRatio * phase)
				+ TopLevel * std::sin(TopRatio * phase);
			const double envelope = Envelope(seconds, static_cast<double>(NpcCueMs) / 1000)
				* std::exp(-seconds / NpcDecaySeconds);
			sample = Level * envelope * (body + NpcTexture * texture);
		} else {
			sample = Whoosh(whoosh, timbre, static_cast<double>(frame) / static_cast<double>(frames - 1), pitch);
		}
		const double scaled = sample * safeGain * PcmPeak;
		pcm[frame * Channels] = static_cast<int16_t>(std::clamp(scaled * left, -PcmPeak, PcmPeak));
		pcm[frame * Channels + 1] = static_cast<int16_t>(std::clamp(scaled * right, -PcmPeak, PcmPeak));
	}
	return pcm;
}
}
