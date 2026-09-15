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
void CueQueue::Retain(const Radar::Events& observations) {
	for (size_t i = 0; i < pending_.size(); ++i)
		if (pending_[i] != observations[i]) pending_[i] = Radar::State::Unknown;
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
std::vector<int16_t> Synthesize(const Radar::Events& events) {
	// Sound-design values, deliberately separate from collision constants.
	constexpr double Duration = Radar::CueDurationMs / 1000.0, Attack = 0.003, Release = 0.015;
	constexpr double OpenHz = 1100, BlockedHz = 260, DownPitch = 0.86;
	constexpr double Level = 0.15, PcmPeak = 32767;
	constexpr std::array<double, Radar::DirectionCount> Pan{0, -0.9, 0.9, 0};
	const size_t frames = static_cast<size_t>(Duration * SampleRate);
	std::vector<int16_t> pcm;
	for (size_t direction = 0; direction < events.size(); ++direction) {
		if (events[direction] == Radar::State::Unknown) continue;
		if (!pcm.empty()) pcm.resize(pcm.size() + SampleRate * Channels * CueGapMs / 1000);
		for (size_t frame = 0; frame < frames; ++frame) {
			const double time = static_cast<double>(frame) / SampleRate;
			const bool open = events[direction] == Radar::State::Open;
			const double hz = (open ? OpenHz : BlockedHz) * (direction == static_cast<size_t>(Radar::Direction::Down) ? DownPitch : 1);
			const double phase = 2 * std::numbers::pi * hz * time;
			const double envelope = std::min(1.0, time / Attack) * std::min(1.0, (Duration - time) / Release)
				* std::exp(-time / (open ? 0.045 : 0.025));
			const double tone = Level * envelope * (std::sin(phase) + 0.2 * std::sin(phase * (open ? 2.7 : 1.5)));
			const double left = tone * std::sqrt((1 - Pan[direction]) / 2);
			const double right = tone * std::sqrt((1 + Pan[direction]) / 2);
			pcm.push_back(static_cast<int16_t>(std::clamp(left, -1.0, 1.0) * PcmPeak));
			pcm.push_back(static_cast<int16_t>(std::clamp(right, -1.0, 1.0) * PcmPeak));
		}
	}
	return pcm;
}
}
