#include "radar_audio.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace D2RAccess::RadarAudio {
void CueQueue::Push(const Radar::Events& events, CueSource source) {
	for (size_t i = 0; i < events.size(); ++i) {
		if (events[i] == Radar::State::Unknown) continue;
		pending_[i] = events[i];
		sources_[i] = source;
	}
}
void CueQueue::Retain(const Radar::Events& states) {
	for (size_t i = 0; i < pending_.size(); ++i)
		if (pending_[i] != states[i]) pending_[i] = Radar::State::Unknown;
}
Radar::Events CueQueue::Take(CueSource* source) {
	Radar::Events result{};
	if (source) *source = {};
	for (size_t offset = 0; offset < pending_.size(); ++offset) {
		const size_t i = (next_ + offset) % pending_.size();
		if (pending_[i] == Radar::State::Unknown) continue;
		result[i] = pending_[i];
		if (source) *source = sources_[i];
		pending_[i] = Radar::State::Unknown;
		next_ = (i + 1) % pending_.size();
		break;
	}
	return result;
}
bool CueQueue::Empty() const {
	return std::all_of(pending_.begin(), pending_.end(), [](auto state) { return state == Radar::State::Unknown; });
}
std::vector<int16_t> Synthesize(const Radar::Events& events, double gain) {
	// The tin/tock the user validated: sound-design values, deliberately
	// separate from collision constants. Bright register = opening, dull
	// register = wall; stereo position follows the screen x component; down
	// is the same tone slightly lower. Volume scales the compiled level.
	constexpr double Duration = Radar::CueDurationMs / 1000.0, Attack = 0.003, Release = 0.015;
	constexpr double OpenHz = 1100, BlockedHz = 260, DownPitch = 0.86;
	constexpr double OpenHarmonic = 2.7, BlockedHarmonic = 1.5, HarmonicLevel = 0.2;
	constexpr double OpenDecay = 0.045, BlockedDecay = 0.025;
	constexpr double PanWidth = 0.9; // left/right sit just inside the stereo edges
	constexpr double Level = 0.15, PcmPeak = 32767;
	const size_t frames = static_cast<size_t>(Duration * SampleRate);
	const double level = Level * std::clamp(gain, 0.0, 1.0);
	std::vector<int16_t> pcm;
	for (size_t direction = 0; direction < events.size(); ++direction) {
		if (events[direction] == Radar::State::Unknown) continue;
		if (!pcm.empty()) pcm.resize(pcm.size() + SampleRate * Channels * CueGapMs / 1000);
		const auto screen = Radar::ScreenAxes[direction];
		const double pan = PanWidth * screen.x;
		const bool open = events[direction] == Radar::State::Open;
		const double hz = (open ? OpenHz : BlockedHz) * (screen.y > 0 ? DownPitch : 1);
		const double harmonic = open ? OpenHarmonic : BlockedHarmonic, decay = open ? OpenDecay : BlockedDecay;
		const double left = std::sqrt((1 - pan) / 2), right = std::sqrt((1 + pan) / 2);
		for (size_t frame = 0; frame < frames; ++frame) {
			const double time = static_cast<double>(frame) / SampleRate;
			const double phase = 2 * std::numbers::pi * hz * time;
			const double envelope = std::min(1.0, time / Attack) * std::min(1.0, (Duration - time) / Release)
				* std::exp(-time / decay);
			const double tone = level * envelope * (std::sin(phase) + HarmonicLevel * std::sin(phase * harmonic));
			pcm.push_back(static_cast<int16_t>(std::clamp(tone * left, -1.0, 1.0) * PcmPeak));
			pcm.push_back(static_cast<int16_t>(std::clamp(tone * right, -1.0, 1.0) * PcmPeak));
		}
	}
	return pcm;
}
}
