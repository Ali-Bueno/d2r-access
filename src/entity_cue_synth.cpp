#include "entity_cue_audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace D2RAccess::EntityCueAudio {
namespace {
// Each pulse stays below the parent's 160 ms repeat interval. The NPC's
// ascending pair is friendly; enemies are lower struck, lightly rough tones.
constexpr unsigned CueMs = 104, NpcNoteMs = 46, NpcGapMs = 8;
constexpr double NpcFirstHz = 660, NpcSecondHz = 880;
constexpr double ImportantHz = 330, ResurrectorHz = 410, RangedHz = 510;
constexpr double RearPitch = 0.80; // a major-third drop makes rear audible without changing the cue identity
constexpr double AttackSeconds = 0.005, ReleaseSeconds = 0.020, EnemyDecaySeconds = 0.055;
constexpr double NpcLevel = 0.27, EnemyLevel = 0.25, PcmPeak = 32767;
constexpr double NpcHarmonic = 0.10, EnemyHarmonic = 0.22, EnemyNoise = 0.055;
constexpr double ResurrectorTremoloHz = 17, RangedTremoloHz = 29;
constexpr double TremoloDepth = 0.28;
constexpr size_t CueFrames = CueMs * SampleRate / 1000;

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
	std::vector<int16_t> pcm(CueFrames * Channels);

	for (size_t frame = 0; frame < CueFrames; ++frame) {
		const double seconds = static_cast<double>(frame) / SampleRate;
		double sample = 0;
		if (sound == Sound::Npc) {
			constexpr size_t NoteFrames = NpcNoteMs * SampleRate / 1000;
			constexpr size_t GapFrames = NpcGapMs * SampleRate / 1000;
			const bool second = frame >= NoteFrames + GapFrames;
			const size_t localFrame = second ? frame - NoteFrames - GapFrames : frame;
			if (localFrame < NoteFrames) {
				const double localSeconds = static_cast<double>(localFrame) / SampleRate;
				const double hz = (second ? NpcSecondHz : NpcFirstHz) * pitch;
				const double phase = 2 * std::numbers::pi * hz * localSeconds;
				const double envelope = Envelope(localSeconds, static_cast<double>(NpcNoteMs) / 1000);
				sample = NpcLevel * envelope * (std::sin(phase) + NpcHarmonic * std::sin(2 * phase));
			}
		} else {
			double hz = ImportantHz, tremoloHz = 0;
			switch (sound) {
		case Sound::Resurrector: hz = ResurrectorHz; tremoloHz = ResurrectorTremoloHz; break;
		case Sound::Ranged: hz = RangedHz; tremoloHz = RangedTremoloHz; break;
		default: break;
		}
			const double phase = 2 * std::numbers::pi * hz * pitch * seconds;
			const double tremolo = tremoloHz == 0 ? 1.0
				: 1.0 - TremoloDepth * (1 - std::cos(2 * std::numbers::pi * tremoloHz * seconds)) / 2;
			const double envelope = Envelope(seconds, static_cast<double>(CueMs) / 1000)
				* std::exp(-seconds / EnemyDecaySeconds);
			sample = EnemyLevel * envelope * tremolo
				* (std::sin(phase) + EnemyHarmonic * std::sin(2 * phase) + EnemyNoise * Noise(frame));
		}
		const double scaled = sample * safeGain * PcmPeak;
		pcm[frame * Channels] = static_cast<int16_t>(std::clamp(scaled * left, -PcmPeak, PcmPeak));
		pcm[frame * Channels + 1] = static_cast<int16_t>(std::clamp(scaled * right, -PcmPeak, PcmPeak));
	}
	return pcm;
}
}
