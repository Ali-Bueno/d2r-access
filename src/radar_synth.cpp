#include "radar_audio.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace D2RAccess::RadarAudio {
namespace {
constexpr double Release = 0.015;
// Floor: noise through a resonant band-pass whose centre rises for an upward
// ray, falls for a downward one and holds for the sides, as in the D4 radar.
// Peak-normalized; a fixed noise sequence keeps the cue reproducible. docs/radar-readings.md
std::vector<double> Breath(double screenY, double peak) {
	constexpr double Duration = Radar::BreathDurationMs / 1000.0, Q = 3;
	constexpr double UpStartHz = 900, UpEndHz = 2400, DownStartHz = 600, DownEndHz = 220, SideHz = 1200;
	const double startHz = screenY < 0 ? UpStartHz : screenY > 0 ? DownStartHz : SideHz;
	const double endHz = screenY < 0 ? UpEndHz : screenY > 0 ? DownEndHz : SideHz;
	std::vector<double> breath(static_cast<size_t>(Duration * SampleRate));
	uint32_t seed = 1;
	double low = 0, band = 0, loudest = 0;
	for (size_t frame = 0; frame < breath.size(); ++frame) {
		const double time = static_cast<double>(frame) / SampleRate;
		const double centreHz = startHz * std::pow(endHz / startHz, time / Duration);
		const double tuning = 2 * std::sin(std::numbers::pi * centreHz / SampleRate); // Chamberlin state-variable filter
		seed = seed * 1664525u + 1013904223u;
		const double high = (seed >> 8) / 8388608.0 - 1 - low - band / Q;
		band += tuning * high;
		low += tuning * band;
		const double swell = std::sin(std::numbers::pi * time / Duration);
		breath[frame] = swell * swell * std::min(1.0, (Duration - time) / Release) * band;
		loudest = std::max(loudest, std::abs(breath[frame]));
	}
	if (loudest > 0)
		for (auto& sample : breath) sample *= peak / loudest;
	return breath;
}
}
void CueQueue::Push(const Radar::Events& events, CueSource source, const Radar::Marks& marks) {
	for (size_t i = 0; i < events.size(); ++i) {
		if (events[i] == Radar::State::Unknown) continue;
		pending_[i] = events[i];
		sources_[i] = source;
		sources_[i].unexplored = marks[i];
	}
}
void CueQueue::Retain(const Radar::Events& states, const Radar::Marks& marks) {
	for (size_t i = 0; i < pending_.size(); ++i)
		if (pending_[i] != states[i] || sources_[i].unexplored != marks[i]) pending_[i] = Radar::State::Unknown;
}
Radar::Events CueQueue::Take(CueSource* source) {
	Radar::Events result{};
	if (source) *source = {};
	// A wall is a warning, an exit is news, floor is background: the most
	// urgent pending direction plays first, round robin among equals.
	const auto urgency = [](Radar::State state) { return Radar::Obstacle(state) ? 2 : state == Radar::State::Exit ? 1 : 0; };
	size_t chosen = pending_.size();
	for (size_t offset = 0; offset < pending_.size(); ++offset) {
		const size_t i = (next_ + offset) % pending_.size();
		if (pending_[i] == Radar::State::Unknown) continue;
		if (chosen == pending_.size() || urgency(pending_[i]) > urgency(pending_[chosen])) chosen = i;
	}
	if (chosen == pending_.size()) return result;
	result[chosen] = pending_[chosen];
	if (source) *source = sources_[chosen];
	pending_[chosen] = Radar::State::Unknown;
	next_ = (chosen + 1) % pending_.size();
	return result;
}
bool CueQueue::Empty() const {
	return std::all_of(pending_.begin(), pending_.end(), [](auto state) { return state == Radar::State::Unknown; });
}
std::vector<int16_t> Synthesize(const Radar::Events& events, const Radar::Marks& marks, double gain, Radar::Frame frame) {
	// Sound-design values, deliberately separate from collision constants.
	// Bright tin = exit, dull tock = wall, a double knock = door, a soft breath
	// of filtered noise = floor that leads nowhere new; stereo position follows
	// the screen x component; down is the same sound slightly lower, and the
	// breath rises up and falls down. A marked (unexplored) exit or floor ends
	// in a double pip after a short gap: the base cue is untouched, the tail is
	// the same for both. `docs/exploration.md`
	constexpr double Duration = Radar::CueDurationMs / 1000.0, Attack = 0.003;
	constexpr double ExitHz = 1100, BlockedHz = 260, DoorHz = 180, DownPitch = 0.86;
	constexpr double ExitHarmonic = 2.7, BlockedHarmonic = 1.5, DoorHarmonic = 2.0, HarmonicLevel = 0.2;
	constexpr double ExitDecay = 0.045, BlockedDecay = 0.025, KnockDecay = 0.012, KnockGap = 0.06;
	constexpr double BreathPeak = 0.7; // of the struck cues' level, as in the D4 radar
	constexpr double PanWidth = 0.9; // left/right sit just inside the stereo edges
	constexpr double Level = 0.17, PcmPeak = 32767;
	constexpr double MarkHz = Radar::UnexploredMarkHz, PipDuration = Radar::UnexploredPipMs / 1000.0;
	constexpr double MarkDecay = 0.03, MarkHarmonic = 2.0;
	constexpr size_t MarkGapFrames = Radar::UnexploredMarkGapMs * SampleRate / 1000;
	constexpr size_t PipFrames = Radar::UnexploredPipMs * SampleRate / 1000;
	constexpr size_t PipGapFrames = Radar::UnexploredPipGapMs * SampleRate / 1000;
	const size_t frames = static_cast<size_t>(Duration * SampleRate);
	constexpr size_t MarkFrames = MarkGapFrames + 2 * PipFrames + PipGapFrames;
	const double level = Level * std::clamp(gain, 0.0, 1.0);
	std::vector<int16_t> pcm;
	for (size_t direction = 0; direction < events.size(); ++direction) {
		const auto state = events[direction];
		if (state == Radar::State::Unknown) continue;
		if (!pcm.empty()) pcm.resize(pcm.size() + SampleRate * Channels * CueGapMs / 1000);
		const auto screen = Radar::RaysOf(frame)[direction].screen;
		const double pan = PanWidth * screen.x, pitch = screen.y > 0 ? DownPitch : 1;
		const double left = std::sqrt((1 - pan) / 2), right = std::sqrt((1 + pan) / 2);
		const bool marked = marks[direction] && Radar::Passable(state);
		const double hz = (state == Radar::State::Exit ? ExitHz : state == Radar::State::Door ? DoorHz : BlockedHz) * pitch;
		const double harmonic = state == Radar::State::Exit ? ExitHarmonic : state == Radar::State::Door ? DoorHarmonic : BlockedHarmonic;
		const double decay = state == Radar::State::Exit ? ExitDecay : state == Radar::State::Door ? KnockDecay : BlockedDecay;
		const bool breathing = state == Radar::State::Open;
		const auto breath = breathing ? Breath(screen.y, level * BreathPeak) : std::vector<double>{};
		for (size_t frame = 0; frame < (breathing ? breath.size() : frames); ++frame) {
			const double time = static_cast<double>(frame) / SampleRate;
			const double fade = std::min(1.0, (Duration - time) / Release);
			double tone = 0;
			if (breathing) {
				tone = breath[frame];
			} else {
				// A door is two strikes of the same knock, the second at KnockGap.
				const double struck = state == Radar::State::Door && time >= KnockGap ? time - KnockGap : time;
				const double phase = 2 * std::numbers::pi * hz * time;
				const double envelope = std::min(1.0, struck / Attack) * fade * std::exp(-struck / decay);
				tone = level * envelope * (std::sin(phase) + HarmonicLevel * std::sin(phase * harmonic));
			}
			pcm.push_back(static_cast<int16_t>(std::clamp(tone * left, -1.0, 1.0) * PcmPeak));
			pcm.push_back(static_cast<int16_t>(std::clamp(tone * right, -1.0, 1.0) * PcmPeak));
		}
		if (!marked) continue;
		for (size_t frame = 0; frame < MarkFrames; ++frame) {
			double tone = 0;
			if (frame >= MarkGapFrames) {
				const size_t pipFrame = (frame - MarkGapFrames) % (PipFrames + PipGapFrames);
				if (pipFrame < PipFrames) {
					const double time = static_cast<double>(pipFrame) / SampleRate;
					const double fade = std::min(1.0, (PipDuration - time) / Release);
					const double phase = 2 * std::numbers::pi * MarkHz * pitch * time;
					const double envelope = std::min(1.0, time / Attack) * fade * std::exp(-time / MarkDecay);
					tone = level * envelope * (std::sin(phase) + HarmonicLevel * std::sin(phase * MarkHarmonic));
				}
			}
			pcm.push_back(static_cast<int16_t>(std::clamp(tone * left, -1.0, 1.0) * PcmPeak));
			pcm.push_back(static_cast<int16_t>(std::clamp(tone * right, -1.0, 1.0) * PcmPeak));
		}
	}
	return pcm;
}
}
