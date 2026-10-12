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

// Ordinary enemies, 2026-10-02: a short rough growl, a buzzy low tone whose
// loudness flutters, so the commonest cue is unlike every swoop and the breath.
constexpr unsigned GrowlMs = 90;
constexpr double GrowlStartHz = 175, GrowlEndHz = 125, GrowlLevel = 0.36;
constexpr double GrowlFlutterHz = 36, GrowlFlutterDepth = 0.55;
constexpr int GrowlHarmonics = 5;
// Attack reach: a bright rising two-note ping, far above every other cue.
constexpr unsigned ChimeMs = 96;
constexpr double ChimeLowHz = 1320, ChimeHighHz = 1760, ChimeSplit = 0.42;
constexpr double ChimeDecaySeconds = 0.03, ChimeLevel = 0.26, ChimeOvertone = 0.22;

// Pickup range (a tester, 2026-10-11): a dry high tick, the shortest cue, like a
// prompt appearing. Inventory edge: a dull wooden knock, panned to the side hit.
constexpr unsigned ClickMs = 30, KnockMs = 70;
constexpr double ClickHz = 2400, ClickDecaySeconds = 0.006, ClickLevel = 0.30, ClickNoise = 0.5;
constexpr double KnockStartHz = 230, KnockEndHz = 150, KnockDecaySeconds = 0.022, KnockLevel = 0.42, KnockNoise = 0.25;

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

double Scale(double value) { return std::isfinite(value) ? std::max(value, 0.0) : 1; }

double Noise(size_t frame) {
	uint32_t hash = static_cast<uint32_t>(frame) * 1664525u + 1013904223u;
	hash ^= hash >> 16;
	return static_cast<double>(hash & 0xffffu) / 32767.5 - 1.0;
}

double Envelope(double seconds, double duration) {
	return std::clamp(seconds / AttackSeconds, 0.0, 1.0)
		* std::clamp((duration - seconds - 1.0 / SampleRate) / ReleaseSeconds, 0.0, 1.0);
}

double Growl(double& phase, double progress, double seconds, double pitch) {
	const double hz = (GrowlStartHz + (GrowlEndHz - GrowlStartHz) * progress) * pitch;
	phase += 2 * std::numbers::pi * hz / SampleRate;
	double body = 0, norm = 0;
	for (int harmonic = 1; harmonic <= GrowlHarmonics; ++harmonic) {
		body += std::sin(harmonic * phase) / harmonic;
		norm += 1.0 / harmonic;
	}
	const double flutter = 1 - GrowlFlutterDepth * (0.5 + 0.5 * std::sin(2 * std::numbers::pi * GrowlFlutterHz * seconds));
	const double swell = std::sin(std::numbers::pi * progress);
	return GrowlLevel * swell * swell * flutter * body / norm;
}

double Chime(double seconds, double pitch) {
	constexpr double Total = static_cast<double>(ChimeMs) / 1000, Split = Total * ChimeSplit;
	const bool high = seconds >= Split;
	const double start = high ? Split : 0, length = high ? Total - Split : Split;
	const double local = seconds - start;
	const double phase = 2 * std::numbers::pi * (high ? ChimeHighHz : ChimeLowHz) * pitch * local;
	const double tone = std::sin(phase) + ChimeOvertone * std::sin(2 * phase);
	return ChimeLevel * Envelope(local, length) * std::exp(-local / ChimeDecaySeconds) * tone;
}

double Click(double seconds, double noise) {
	const double tone = std::sin(2 * std::numbers::pi * ClickHz * seconds);
	return ClickLevel * Envelope(seconds, static_cast<double>(ClickMs) / 1000) * std::exp(-seconds / ClickDecaySeconds)
		* (tone + ClickNoise * noise);
}

double Knock(double& phase, double seconds, double noise) {
	constexpr double Total = static_cast<double>(KnockMs) / 1000;
	phase += 2 * std::numbers::pi * KnockStartHz * std::pow(KnockEndHz / KnockStartHz, seconds / Total) / SampleRate;
	return KnockLevel * Envelope(seconds, Total) * std::exp(-seconds / KnockDecaySeconds) * (std::sin(phase) + KnockNoise * noise);
}
}

std::vector<int16_t> Synthesize(Sound sound, Placement placement, double gain) {
	const double safeGain = std::isfinite(gain) ? std::clamp(gain, 0.0, 1.0) : 0;
	const double pan = std::clamp(SafeAxis(placement.right, 0) * Scale(placement.panScale), -1.0, 1.0);
	const double front = SafeAxis(placement.front, 1);
	const double pitch = 1 + (RearPitch - 1) * std::clamp(-front, 0.0, 1.0) * Scale(placement.pitchScale);
	const double left = std::sqrt((1 - pan) / 2);
	const double right = std::sqrt((1 + pan) / 2);
	const auto timbre = Voice(sound);
	const bool npc = sound == Sound::Npc;
	const unsigned milliseconds = npc ? NpcCueMs : sound == Sound::Ordinary ? GrowlMs : sound == Sound::Reach ? ChimeMs
		: sound == Sound::Pickup ? ClickMs : sound == Sound::Edge ? KnockMs : WhooshMs;
	const size_t frames = milliseconds * SampleRate / 1000;
	double growlPhase = 0;
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
		} else if (sound == Sound::Ordinary) {
			sample = Growl(growlPhase, static_cast<double>(frame) / static_cast<double>(frames - 1), seconds, pitch);
		} else if (sound == Sound::Reach) {
			sample = Chime(seconds, pitch);
		} else if (sound == Sound::Pickup || sound == Sound::Edge) {
			texture += textureBlend * (Noise(frame) - texture);
			sample = sound == Sound::Pickup ? Click(seconds, texture) : Knock(growlPhase, seconds, texture);
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
