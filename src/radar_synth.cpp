#include "radar_audio.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace D2RAccess::RadarAudio {
std::vector<int16_t> Synthesize(const Radar::Events& events) {
	// Sound-design values, deliberately separate from collision constants.
	constexpr double Duration = 0.12, Attack = 0.003, Release = 0.015;
	constexpr double OpenHz = 1100, BlockedHz = 260, RearPitch = 0.86;
	constexpr double Level = 0.15, PcmPeak = 32767;
	constexpr std::array<double, Radar::DirectionCount> Pan{0, -0.9, 0.9, 0};
	const size_t frames = static_cast<size_t>(Duration * SampleRate);
	std::vector<int16_t> pcm(frames * Channels);
	for (size_t frame = 0; frame < frames; ++frame) {
		const double time = static_cast<double>(frame) / SampleRate;
		double left = 0, right = 0;
		for (size_t direction = 0; direction < events.size(); ++direction) {
			if (events[direction] == Radar::State::Unknown) continue;
			const bool open = events[direction] == Radar::State::Open;
			const double hz = (open ? OpenHz : BlockedHz) * (direction == static_cast<size_t>(Radar::Direction::Back) ? RearPitch : 1);
			const double phase = 2 * std::numbers::pi * hz * time;
			const double envelope = std::min(1.0, time / Attack) * std::min(1.0, (Duration - time) / Release)
				* std::exp(-time / (open ? 0.045 : 0.025));
			const double tone = Level * envelope * (std::sin(phase) + 0.2 * std::sin(phase * (open ? 2.7 : 1.5)));
			left += tone * std::sqrt((1 - Pan[direction]) / 2);
			right += tone * std::sqrt((1 + Pan[direction]) / 2);
		}
		pcm[frame * Channels] = static_cast<int16_t>(std::clamp(left, -1.0, 1.0) * PcmPeak);
		pcm[frame * Channels + 1] = static_cast<int16_t>(std::clamp(right, -1.0, 1.0) * PcmPeak);
	}
	return pcm;
}
}
