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
constexpr double MinPitch = 0.25;  // two octaves down: the strongest pitch strength still leaves a tone
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
// Enemy classes by timbre (a tester, 2026-10-13): pitch already says up or down,
// so every enemy growls in the same register and its class adds a layer over it,
// tuned from the growl's middle frequency. Each swells with the growl, so an enemy never
// strikes like the NPC tap or the reach ping, at about half the growl's level (RMS) so the
// growl stays the base. See docs/entity-cues.md.
constexpr double LayerBaseHz = (GrowlStartHz + GrowlEndHz) / 2;
// Elite: a raspy snarl, a bright buzz chattering like teeth; tonal, never the radar's noise. 750 Hz and
// harmonic phases spread to keep the crest low (1.8 kHz centroid) so it cuts through six growls; docs/entity-cues.md.
constexpr double TeethRatio = 5, TeethChatterHz = 62, TeethLevel = 0.28, TeethRolloff = 1, TeethPhaseSpread = 1.3;
constexpr int TeethHarmonics = 6;
// Boss: a metallic ring, a struck bar's inharmonic partials above the growl.
constexpr double RingRatios[] = {2.76, 5.40, 8.93}, RingWeights[] = {1, 0.55, 0.3};
constexpr double RingLevel = 0.08;
// Raiser of the dead: a wailing chant, a tone with a fast vibrato that glides up one
// semitone, rising as the dead do (a tester, 2026-10-17: a semitone at most).
constexpr double ChantRatio = 4, ChantVibratoHz = 22, ChantVibratoDepth = 0.06, ChantLevel = 0.1;
inline const double ChantGlide = std::pow(2.0, 1.0 / 12) - 1;
// Ranged: a thin high whistle, far above the chant and with no vibrato.
constexpr double WhistleRatio = 11, WhistleLevel = 0.07;
// Attack reach: a bright rising two-note ping, far above every other cue.
constexpr unsigned ChimeMs = 96;
constexpr double ChimeLowHz = 1320, ChimeHighHz = 1760, ChimeSplit = 0.42;
constexpr double ChimeDecaySeconds = 0.03, ChimeLevel = 0.26, ChimeOvertone = 0.22;

// The "!" over a town NPC (a tester, 2026-10-13): a small bell struck once,
// one note unlike the attack reach's rising two.
constexpr unsigned DingMs = 180;
constexpr double DingHz = 1046.5, DingDecaySeconds = 0.06, DingLevel = 0.24;
constexpr double DingRatios[] = {1, 2.0, 2.76}, DingWeights[] = {1, 0.35, 0.2};

// Pickup range (a tester, 2026-10-11): a dry high tick, the shortest cue, like a
// prompt appearing. Inventory edge: a dull wooden knock, panned to the side hit.
constexpr unsigned ClickMs = 30, KnockMs = 70;
constexpr double ClickHz = 2400, ClickDecaySeconds = 0.006, ClickLevel = 0.30, ClickNoise = 0.5;
constexpr double KnockStartHz = 230, KnockEndHz = 150, KnockDecaySeconds = 0.022, KnockLevel = 0.42, KnockNoise = 0.25;

// Usable corpses near a Necromancer (a tester, 2026-10-14): three quick hollow
// taps falling in pitch, bones rattling, higher and drier than the inventory knock.
constexpr unsigned RattleMs = 150;
constexpr double RattleHz[] = {620, 520, 440};
constexpr double RattleDecaySeconds = 0.012, RattleLevel = 0.34, RattleNoise = 0.4;

// The game's target on an enemy (Raine, 2026-10-22, after Shades of Doom's targeting
// sensor): two flat electronic blips, odd harmonics like a square wave, unlike the
// reach ping's two rising sine notes and the pickup's single noisy tick.
constexpr unsigned SightMs = 64;
constexpr double SightHz = 1175, SightBlipSeconds = 0.024, SightGapSeconds = 0.016, SightLevel = 0.2;
constexpr double SightHarmonics[] = {1, 3, 5}, SightWeights[] = {1, 0.33, 0.2};

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

// The layer a class adds over the growl; zero for an ordinary enemy.
double ClassLayer(Sound sound, double progress, double seconds, double pitch) {
	const double base = LayerBaseHz * pitch, rise = std::sin(std::numbers::pi * progress), swell = rise * rise; // the growl's own
	const auto tone = [&](double ratio) { return std::sin(2 * std::numbers::pi * base * ratio * seconds); };
	switch (sound) {
	case Sound::Important: {
		const double chatter = std::max(0.0, std::sin(2 * std::numbers::pi * TeethChatterHz * seconds));
		double buzz = 0;
		for (int harmonic = 1; harmonic <= TeethHarmonics; ++harmonic)
			buzz += std::sin(2 * std::numbers::pi * base * TeethRatio * harmonic * seconds + TeethPhaseSpread * harmonic * harmonic) / std::pow(harmonic, TeethRolloff);
		return TeethLevel * swell * chatter * chatter * buzz;
	}
	case Sound::Boss: {
		double ring = 0;
		for (size_t i = 0; i < std::size(RingRatios); ++i) ring += RingWeights[i] * tone(RingRatios[i]);
		return RingLevel * swell * ring;
	}
	case Sound::Resurrector: {
		const double wobble = ChantVibratoDepth / (2 * std::numbers::pi * ChantVibratoHz) * std::sin(2 * std::numbers::pi * ChantVibratoHz * seconds);
		const double glide = ChantGlide * seconds * progress / 2; // the phase of a pitch rising linearly over the cue
		return ChantLevel * swell * std::sin(2 * std::numbers::pi * base * ChantRatio * (seconds + glide + wobble));
	}
	case Sound::Ranged: return WhistleLevel * swell * tone(WhistleRatio);
	default: return 0;
	}
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

double Sight(double seconds, double pitch) {
	constexpr double Period = SightBlipSeconds + SightGapSeconds;
	const double local = seconds >= Period ? seconds - Period : seconds;
	if (local >= SightBlipSeconds) return 0;
	double tone = 0;
	for (size_t i = 0; i < std::size(SightHarmonics); ++i)
		tone += SightWeights[i] * std::sin(2 * std::numbers::pi * SightHz * SightHarmonics[i] * pitch * local);
	return SightLevel * Envelope(local, SightBlipSeconds) * tone;
}

double Ding(double seconds, double pitch) {
	double bell = 0;
	for (size_t i = 0; i < std::size(DingRatios); ++i)
		bell += DingWeights[i] * std::sin(2 * std::numbers::pi * DingHz * DingRatios[i] * pitch * seconds)
			* std::exp(-seconds * (i + 1) / DingDecaySeconds);
	return DingLevel * Envelope(seconds, static_cast<double>(DingMs) / 1000) * bell;
}

double Click(double seconds, double noise) {
	const double tone = std::sin(2 * std::numbers::pi * ClickHz * seconds);
	return ClickLevel * Envelope(seconds, static_cast<double>(ClickMs) / 1000) * std::exp(-seconds / ClickDecaySeconds)
		* (tone + ClickNoise * noise);
}

double Rattle(double seconds, double noise, double pitch) {
	constexpr double Total = static_cast<double>(RattleMs) / 1000, Tap = Total / std::size(RattleHz);
	const size_t tap = std::min(static_cast<size_t>(seconds / Tap), std::size(RattleHz) - 1);
	const double local = seconds - static_cast<double>(tap) * Tap;
	return RattleLevel * Envelope(seconds, Total) * std::exp(-local / RattleDecaySeconds)
		* (std::sin(2 * std::numbers::pi * RattleHz[tap] * pitch * local) + RattleNoise * noise);
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
	const double pitch = std::max(MinPitch, 1 + (RearPitch - 1) * std::clamp(-front, 0.0, 1.0) * Scale(placement.pitchScale));
	const double left = std::sqrt((1 - pan) / 2);
	const double right = std::sqrt((1 + pan) / 2);
	const auto timbre = Voice(sound);
	const bool npc = sound == Sound::Npc;
	const bool enemy = sound == Sound::Important || sound == Sound::Boss || sound == Sound::Resurrector
		|| sound == Sound::Ranged || sound == Sound::Ordinary;
	const bool growl = sound == Sound::Ordinary || (enemy && placement.classLayers);
	const unsigned milliseconds = npc ? NpcCueMs : growl ? GrowlMs : sound == Sound::Reach ? ChimeMs
		: sound == Sound::Pickup ? ClickMs : sound == Sound::Edge ? KnockMs : sound == Sound::NpcAlert ? DingMs
		: sound == Sound::Corpse ? RattleMs : sound == Sound::Sight ? SightMs : WhooshMs;
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
		} else if (growl) {
			const double progress = static_cast<double>(frame) / static_cast<double>(frames - 1);
			sample = Growl(growlPhase, progress, seconds, pitch) + ClassLayer(sound, progress, seconds, pitch);
		} else if (sound == Sound::Reach) {
			sample = Chime(seconds, pitch);
		} else if (sound == Sound::NpcAlert) {
			sample = Ding(seconds, pitch);
		} else if (sound == Sound::Sight) {
			sample = Sight(seconds, pitch);
		} else if (sound == Sound::Pickup || sound == Sound::Edge || sound == Sound::Corpse) {
			texture += textureBlend * (Noise(frame) - texture);
			sample = sound == Sound::Pickup ? Click(seconds, texture) : sound == Sound::Corpse ? Rattle(seconds, texture, pitch)
				: Knock(growlPhase, seconds, texture);
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
