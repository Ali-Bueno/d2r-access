#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>

// The Assassin's charges as a chord, a note per charge (bfbeast, 2026-11-02): soft synthesized
// tones, struck as a charge is gained or held while they last. docs/assassin-charges.md
namespace D2RAccess::ChargeChord {

// Equal-tempered A3, C4 and E4: A minor built from the root (the user's pick, 2026-11-02, over
// bfbeast's C major example).
inline constexpr std::array<double, 3> NoteHz{220.00, 261.63, 329.63};
inline constexpr int NoteCount = static_cast<int>(NoteHz.size());
inline constexpr double AttackMs = 30; // a soft onset that never clicks
inline constexpr double DecayMs = 350; // time constant: a struck chord is gone in about a second and a half
inline constexpr double HeldLevel = 0.5; // a held chord settles to half its strike, a bed under the fight
inline constexpr double NoteGain = 0.12; // each note's peak: three stay well under half of full scale
inline constexpr double Overtone = 0.2; // the octave above, so small speakers still carry it
inline constexpr double SilentLevel = 1e-3; // -60 dB, a few steps of the 16-bit output: a fading note stops here

class Synth {
public:
	explicit Synth(double sampleRate)
		: m_attack(1000 / (AttackMs * sampleRate)), m_decay(std::exp(-1000 / (DecayMs * sampleRate))) {
		for (size_t i = 0; i < m_voices.size(); ++i) m_voices[i].step = 2 * std::numbers::pi * NoteHz[i] / sampleRate;
	}

	// The first `notes` notes struck together, then fading out.
	void Strike(int notes) {
		for (int i = 0; i < std::min(notes, NoteCount); ++i) m_voices[i].rising = true, m_voices[i].held = false;
	}
	// The first `notes` notes sounding until told otherwise; a note already held is not struck again.
	void Hold(int notes) {
		for (int i = 0; i < NoteCount; ++i) {
			auto& voice = m_voices[i];
			const bool wanted = i < notes;
			if (wanted && !voice.held) voice.rising = true;
			voice.held = wanted;
		}
	}
	bool Silent() const {
		return std::all_of(m_voices.begin(), m_voices.end(), [](const Voice& v) { return !v.rising && !v.held && v.level == 0; });
	}
	void Render(std::span<int16_t> out) {
		for (auto& sample : out) {
			double sum = 0;
			for (auto& voice : m_voices) sum += voice.Next(m_attack, m_decay);
			sample = static_cast<int16_t>(std::clamp(sum * NoteGain / (1 + Overtone) * 32767, -32767.0, 32767.0));
		}
	}

private:
	struct Voice {
		double step = 0, phase = 0, level = 0;
		bool rising = false, held = false;
		double Next(double attack, double decay) {
			if (rising) {
				level += attack;
				if (level >= 1) level = 1, rising = false;
			} else {
				const double floor = held ? HeldLevel : 0;
				level = floor + (level - floor) * decay;
				if (!held && level < SilentLevel) level = 0;
			}
			if (level == 0) return 0;
			const double value = level * (std::sin(phase) + Overtone * std::sin(2 * phase));
			phase = std::fmod(phase + step, 2 * std::numbers::pi);
			return value;
		}
	};
	double m_attack, m_decay;
	std::array<Voice, NoteHz.size()> m_voices{};
};

}
