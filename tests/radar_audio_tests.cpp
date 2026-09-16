#include "radar_audio.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace D2RAccess;
using namespace RadarAudio;
namespace {
constexpr size_t Index(Radar::Direction direction) { return static_cast<size_t>(direction); }
constexpr size_t Up = Index(Radar::Direction::Up), Left = Index(Radar::Direction::Left);
constexpr size_t Right = Index(Radar::Direction::Right), Down = Index(Radar::Direction::Down);
// One cue is CueDurationMs of interleaved stereo; the silent gap that keeps
// successive directions separable is CueGapMs of the same stream.
constexpr size_t CueFrames = Radar::CueDurationMs * SampleRate / 1000;
constexpr size_t CueSamples = CueFrames * Channels;
constexpr size_t GapSamples = SampleRate * Channels * CueGapMs / 1000;

Radar::Events One(size_t direction, Radar::State state) {
	Radar::Events events{};
	events[direction] = state;
	return events;
}
double Rms(const std::vector<int16_t>& pcm, size_t channel, size_t fromFrame, size_t toFrame) {
	double total = 0;
	for (size_t frame = fromFrame; frame < toFrame; ++frame) {
		const double sample = pcm[frame * Channels + channel];
		total += sample * sample;
	}
	return std::sqrt(total / static_cast<double>(toFrame - fromFrame));
}
int Peak(const std::vector<int16_t>& pcm) {
	int peak = 0;
	for (const auto sample : pcm) peak = std::max(peak, std::abs(static_cast<int>(sample)));
	return peak;
}
// Frequency from zero crossings. The cue is one sine plus a quiet harmonic at
// a fifth of its amplitude, so the sum still crosses zero exactly twice per
// period of the fundamental.
double Frequency(const std::vector<int16_t>& pcm, size_t channel, size_t fromFrame, size_t toFrame) {
	int crossings = 0, sign = 0;
	for (size_t frame = fromFrame; frame < toFrame; ++frame) {
		const int sample = pcm[frame * Channels + channel];
		if (sample == 0) continue;
		const int current = sample > 0 ? 1 : -1;
		if (sign != 0 && current != sign) ++crossings;
		sign = current;
	}
	return crossings * static_cast<double>(SampleRate) / (2.0 * static_cast<double>(toFrame - fromFrame));
}

void Layout() {
	const auto single = Synthesize(One(Up, Radar::State::Open));
	Check(single.size() == CueSamples, "one cue lasts exactly the compiled cue duration in stereo frames");
	Radar::Events pair{};
	pair[Up] = Radar::State::Open;
	pair[Down] = Radar::State::Blocked;
	const auto both = Synthesize(pair);
	Check(both.size() == 2 * CueSamples + GapSamples,
		"two cues are played one after the other with the compiled gap between them");
	const auto second = Synthesize(One(Down, Radar::State::Blocked));
	bool identical = std::equal(single.begin(), single.end(), both.begin());
	identical &= std::equal(second.begin(), second.end(), both.end() - static_cast<long long>(CueSamples));
	Check(identical, "batching directions never changes the cue each one produces on its own");
	Check(std::all_of(both.begin() + CueSamples, both.end() - static_cast<long long>(CueSamples),
		[](int16_t sample) { return sample == 0; }), "the gap between two cues is silent");
	Check(Synthesize({}).empty(), "an event set with nothing confirmed produces no audio at all");
}

void StereoPosition() {
	const auto left = Synthesize(One(Left, Radar::State::Open));
	const auto right = Synthesize(One(Right, Radar::State::Open));
	const auto up = Synthesize(One(Up, Radar::State::Open));
	const auto down = Synthesize(One(Down, Radar::State::Open));
	Check(Rms(left, 0, 0, CueFrames) > Rms(left, 1, 0, CueFrames), "the left cue is louder in the left ear");
	Check(Rms(right, 1, 0, CueFrames) > Rms(right, 0, 0, CueFrames), "the right cue is louder in the right ear");
	// Equal-power panning at the compiled width: the quiet side keeps a real,
	// much smaller share rather than being switched off.
	const double ratio = Rms(left, 0, 0, CueFrames) / Rms(left, 1, 0, CueFrames);
	Check(ratio > 3 && ratio < 6, "a side cue keeps an audible, clearly smaller share in the far ear");
	Check(std::abs(Rms(right, 1, 0, CueFrames) / Rms(right, 0, 0, CueFrames) - ratio) < 1e-6,
		"the two sides are mirror images of each other");
	bool mirrored = true;
	for (size_t frame = 0; frame < CueFrames; ++frame)
		mirrored &= left[frame * Channels] == right[frame * Channels + 1]
			&& left[frame * Channels + 1] == right[frame * Channels];
	Check(mirrored, "left and right are the same sound with the channels swapped");
	bool centred = true;
	for (size_t frame = 0; frame < CueFrames; ++frame)
		centred &= up[frame * Channels] == up[frame * Channels + 1]
			&& down[frame * Channels] == down[frame * Channels + 1];
	Check(centred, "up and down are centred, with both ears carrying the same signal");
}

void Register() {
	// Measure over the loud part of the cue, where the decay still leaves a
	// clean waveform in every register.
	constexpr size_t Window = CueFrames / 2;
	const auto openUp = Synthesize(One(Up, Radar::State::Open));
	const auto blockedUp = Synthesize(One(Up, Radar::State::Blocked));
	const auto openDown = Synthesize(One(Down, Radar::State::Open));
	const auto blockedDown = Synthesize(One(Down, Radar::State::Blocked));
	const double open = Frequency(openUp, 0, 0, Window), blocked = Frequency(blockedUp, 0, 0, Window);
	Check(open > 2 * blocked, "an opening sits in a much brighter register than a wall");
	// No glide: the tone holds its pitch for the whole cue.
	Check(std::abs(Frequency(openUp, 0, CueFrames / 2, CueFrames - CueFrames / 8) - open) / open < 0.05,
		"the opening cue holds one pitch from start to end");
	const double downOpen = Frequency(openDown, 0, 0, Window);
	Check(downOpen < open, "down is the same tone a little lower, so it is told apart from up");
	Check(std::abs(downOpen / open - 0.86) < 0.03, "down is lowered by the compiled fraction of a tone, not an octave");
	Check(Frequency(blockedDown, 0, 0, Window) < blocked, "the wall cue is lowered for down in the same way");
	Check(std::abs(Frequency(Synthesize(One(Left, Radar::State::Open)), 0, 0, Window) - open) < 1e-9
		&& std::abs(Frequency(Synthesize(One(Right, Radar::State::Open)), 1, 0, Window) - open) < 1e-9,
		"only down is retuned; the sides keep the opening register");
	// The wall is a shorter, duller tock: it dies away faster than an opening.
	const auto decay = [](const std::vector<int16_t>& pcm) {
		return Rms(pcm, 0, CueFrames / 2, CueFrames) / Rms(pcm, 0, 0, CueFrames / 2);
	};
	Check(decay(blockedUp) < decay(openUp), "the wall tock decays faster than the opening tin");
}

void Volume() {
	const auto full = Synthesize(One(Up, Radar::State::Open), 1);
	const auto half = Synthesize(One(Up, Radar::State::Open), 0.5);
	const auto silent = Synthesize(One(Up, Radar::State::Open), 0);
	Check(silent.size() == full.size(), "a silenced cue still occupies its slot in the stream");
	Check(std::all_of(silent.begin(), silent.end(), [](int16_t sample) { return sample == 0; }),
		"volume zero produces silence, not a quiet cue");
	Check(Peak(full) > 0 && std::abs(static_cast<double>(Peak(half)) / Peak(full) - 0.5) < 0.01,
		"half volume is half the peak level");
	Check(Peak(full) < 32767, "the compiled peak level never reaches full scale");
	Check(Peak(Synthesize(One(Up, Radar::State::Open), 5)) == Peak(full),
		"a volume beyond the maximum is clamped instead of distorting");
}

void Queue() {
	CueQueue queue;
	Check(queue.Empty(), "a fresh queue has nothing to play");
	Radar::Events pair{};
	pair[Left] = Radar::State::Open;
	pair[Down] = Radar::State::Blocked;
	queue.Push(pair, {.eventMs = 1000, .level = 3});
	Check(!queue.Empty(), "pushed events are pending");
	CueSource source{};
	auto taken = queue.Take(&source);
	Check(taken[Left] == Radar::State::Open && taken[Down] == Radar::State::Unknown,
		"the worker is handed one direction at a time");
	Check(source.eventMs == 1000 && source.level == 3, "a taken cue carries the source it was pushed with");
	Check(!queue.Empty(), "the second direction stays queued independently");
	taken = queue.Take();
	Check(taken[Down] == Radar::State::Blocked && taken[Left] == Radar::State::Unknown && queue.Empty(),
		"the queue empties one direction per take");
	CueSource stale{.eventMs = 7};
	taken = queue.Take(&stale);
	Check(std::all_of(taken.begin(), taken.end(), [](Radar::State state) { return state == Radar::State::Unknown; })
		&& stale.eventMs == 0, "taking from an empty queue yields nothing and clears the source");
	Radar::Events two{};
	two[Up] = two[Right] = Radar::State::Open;
	queue.Push(two);
	Check(queue.Take()[Up] == Radar::State::Open, "the round robin resumes from the direction after the last one played");
	Radar::Events again{};
	again[Up] = Radar::State::Blocked;
	queue.Push(again);
	Check(queue.Take()[Right] == Radar::State::Open,
		"a direction that just played waits its turn behind one that has been waiting");
	Check(queue.Take()[Up] == Radar::State::Blocked && queue.Empty(), "the round robin then comes back round");
	queue.Push(One(Down, Radar::State::Open), {.eventMs = 10});
	queue.Push(One(Down, Radar::State::Blocked), {.eventMs = 20});
	Check(queue.Pending()[Down] == Radar::State::Blocked && queue.Source(Down).eventMs == 20,
		"a direction keeps only its latest state, with the source that produced it");
	queue.Push(One(Left, Radar::State::Open));
	Radar::Events confirmed{};
	confirmed[Down] = Radar::State::Blocked;
	confirmed[Left] = Radar::State::Blocked;
	queue.Retain(confirmed);
	Check(queue.Pending()[Down] == Radar::State::Blocked && queue.Pending()[Left] == Radar::State::Unknown,
		"a queued cue that no longer describes the confirmed state of its direction is dropped");
}
}
int main() {
	Layout();
	StereoPosition();
	Register();
	Volume();
	Queue();
	Passed();
}
