#include "radar_audio.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace D2RAccess::RadarAudio {
namespace {
constexpr double Release = 0.015;
// Preserve this user's accepted struck-cue level and stereo width during the
// upstream merge; upstream's separate tester loudness change is not applied.
constexpr double CueLevel = 0.17, PanWidth = 0.9, PcmPeak = 32767;
// Keep the original door level; new bump/clutter sounds remain independent.
constexpr double DoorLevel = 1.0;
// A fixed noise sequence keeps every cue reproducible.
double Noise(uint32_t& seed) {
	seed = seed * 1664525u + 1013904223u;
	return (seed >> 8) / 8388608.0 - 1;
}
// Preserve the user's accepted fixed low-pass floor breath. The darker rear
// cue retains matched power; upstream bump/clutter cues stay independent.
std::vector<double> Breath(double screenY, double level) {
    constexpr double Duration = Radar::CueDurationMs / 1000.0;
    constexpr double CutoffHz = 700, RearPitch = 0.55, BreathLevel = 2.4;
    const double pitch = screenY > 0 ? RearPitch : 1;
    const double gain = BreathLevel / std::sqrt(pitch);
    const double smoothing = 1 - std::exp(-2 * std::numbers::pi * CutoffHz * pitch / SampleRate);
    std::vector<double> samples(static_cast<size_t>(Duration * SampleRate));
    uint32_t seed = 1;
    double filtered = 0;
    for (size_t frame = 0; frame < samples.size(); ++frame) {
        const double time = static_cast<double>(frame) / SampleRate;
        filtered += smoothing * (Noise(seed) - filtered);
        const double swell = std::sin(std::numbers::pi * time / Duration);
        samples[frame] = level * gain * swell * swell * std::min(1.0, (Duration - time) / Release) * filtered;
    }
    return samples;
}
}
void CueQueue::Push(const Radar::Events& events, CueSource source, const Radar::Marks& marks, const Radar::Marks& small) {
	for (size_t i = 0; i < events.size(); ++i) {
		if (events[i] == Radar::State::Unknown) continue;
		pending_[i] = events[i];
		sources_[i] = source;
		sources_[i].unexplored = marks[i];
		sources_[i].small = small[i];
	}
}
void CueQueue::PushBump(double pan, CueSource source) {
	bump_ = true;
	bumpPan_ = pan;
	bumpSource_ = source;
}
bool CueQueue::TakeBump(double* pan, CueSource* source) {
	if (!bump_) return false;
	if (pan) *pan = bumpPan_;
	if (source) *source = bumpSource_;
	bump_ = false;
	return true;
}
void CueQueue::Retain(const Radar::Events& states, const Radar::Marks& marks) {
	// An audition (the sound test, the glossary) describes no reading, so none outdates it.
	for (size_t i = 0; i < pending_.size(); ++i)
		if (!sources_[i].audition && (pending_[i] != states[i] || sources_[i].unexplored != marks[i]))
			pending_[i] = Radar::State::Unknown;
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
	return !bump_ && std::all_of(pending_.begin(), pending_.end(), [](auto state) { return state == Radar::State::Unknown; });
}
std::vector<int16_t> Synthesize(const Radar::Events& events, const Radar::Marks& marks, double gain, Radar::Frame frame,
	const Radar::Marks& small) {
	// Sound-design values, deliberately separate from collision constants.
	// Bright tin = exit, dull tock = wall, a short hollow wooden knock = small
	// obstacle, a double knock = door, a soft breath of filtered noise = floor
	// that leads nowhere new; stereo position follows the screen x component;
	// down is the same sound slightly lower, with a darker floor breath. A marked (unexplored) exit or floor ends in a double pip after a
	// short gap: the base cue is untouched, the tail is the same for both.
	// `docs/exploration.md`, `docs/radar-readings.md`
	constexpr double Duration = Radar::CueDurationMs / 1000.0, Attack = 0.003;
	constexpr double ExitHz = 1100, BlockedHz = 260, SmallHz = 640, DoorHz = 180, DownPitch = 0.86;
	constexpr double ExitHarmonic = 2.7, BlockedHarmonic = 1.5, SmallHarmonic = 2.4, DoorHarmonic = 2.0, HarmonicLevel = 0.2;
	constexpr double ExitDecay = 0.045, BlockedDecay = 0.025, SmallDecay = 0.012, KnockDecay = 0.012, KnockGap = 0.06;
	constexpr double MarkHz = Radar::UnexploredMarkHz, PipDuration = Radar::UnexploredPipMs / 1000.0;
	constexpr double MarkDecay = 0.03, MarkHarmonic = 2.0;
	constexpr size_t MarkGapFrames = Radar::UnexploredMarkGapMs * SampleRate / 1000;
	constexpr size_t PipFrames = Radar::UnexploredPipMs * SampleRate / 1000;
	constexpr size_t PipGapFrames = Radar::UnexploredPipGapMs * SampleRate / 1000;
	const size_t frames = static_cast<size_t>(Duration * SampleRate);
	constexpr size_t MarkFrames = MarkGapFrames + 2 * PipFrames + PipGapFrames;
	const double level = CueLevel * std::clamp(gain, 0.0, 1.0);
	std::vector<int16_t> pcm;
	for (size_t direction = 0; direction < events.size(); ++direction) {
		const auto state = events[direction];
		if (state == Radar::State::Unknown) continue;
		if (!pcm.empty()) pcm.resize(pcm.size() + SampleRate * Channels * CueGapMs / 1000);
		const auto screen = Radar::RaysOf(frame)[direction].screen;
		const double pan = PanWidth * screen.x, pitch = screen.y > 0 ? DownPitch : 1;
		const double left = std::sqrt((1 - pan) / 2), right = std::sqrt((1 + pan) / 2);
		const bool marked = marks[direction] && Radar::Passable(state);
		const bool clutter = small[direction] && state == Radar::State::Blocked;
		const double hz = (state == Radar::State::Exit ? ExitHz : state == Radar::State::Door ? DoorHz : clutter ? SmallHz : BlockedHz) * pitch;
		const double harmonic = state == Radar::State::Exit ? ExitHarmonic : state == Radar::State::Door ? DoorHarmonic
			: clutter ? SmallHarmonic : BlockedHarmonic;
		const double decay = state == Radar::State::Exit ? ExitDecay : state == Radar::State::Door ? KnockDecay
			: clutter ? SmallDecay : BlockedDecay;
		const bool breathing = state == Radar::State::Open;
		const auto breath = breathing ? Breath(screen.y, level) : std::vector<double>{};
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
				const double strike = state == Radar::State::Door ? level * DoorLevel : level;
				tone = strike * envelope * (std::sin(phase) + HarmonicLevel * std::sin(phase * harmonic));
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
std::vector<int16_t> SynthesizeBump(double pan, double gain) {
	// Sound design, ported from the Last Epoch mod's BumpCueSynthesizer: a
	// falling sub-bass body under a short muffled noise click.
	constexpr size_t DurationMs = 90, Frames = DurationMs * SampleRate / 1000;
	constexpr double Duration = DurationMs / 1000.0, Attack = 0.002, BumpRelease = 0.012;
	constexpr double StartHz = 150, EndHz = 70, BodyDecay = 0.030;
	constexpr double ClickDecay = 0.006, ClickLevel = 0.5, ClickCutoffHz = 1800;
	// Low frequencies sound quieter at the same peak: this matches the tock's loudness.
	constexpr double LevelOverKnock = 1.4;
	constexpr uint32_t NoiseSeed = 7;
	const double level = CueLevel * LevelOverKnock * std::clamp(gain, 0.0, 1.0);
	const double side = std::clamp(pan, -1.0, 1.0) * PanWidth;
	const double left = std::sqrt((1 - side) / 2), right = std::sqrt((1 + side) / 2);
	const double smoothing = 1 - std::exp(-2 * std::numbers::pi * ClickCutoffHz / SampleRate); // one-pole low-pass
	std::vector<int16_t> pcm;
	pcm.reserve(Frames * Channels);
	double phase = 0, noise = 0;
	uint32_t seed = NoiseSeed;
	for (size_t frame = 0; frame < Frames; ++frame) {
		const double time = static_cast<double>(frame) / SampleRate;
		phase += 2 * std::numbers::pi * StartHz * std::pow(EndHz / StartHz, time / Duration) / SampleRate;
		noise += smoothing * (Noise(seed) - noise);
		const double gate = std::min(1.0, time / Attack) * std::min(1.0, (Duration - time) / BumpRelease);
		const double value = level * gate * (std::sin(phase) * std::exp(-time / BodyDecay) + ClickLevel * noise * std::exp(-time / ClickDecay));
		pcm.push_back(static_cast<int16_t>(std::clamp(value * left, -1.0, 1.0) * PcmPeak));
		pcm.push_back(static_cast<int16_t>(std::clamp(value * right, -1.0, 1.0) * PcmPeak));
	}
	return pcm;
}
}
