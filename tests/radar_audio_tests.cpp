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
// The floor breath is longer than a struck cue so its sweep can be heard.
constexpr size_t BreathFrames = Radar::BreathDurationMs * SampleRate / 1000;
constexpr size_t BreathSamples = BreathFrames * Channels;

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
	const auto single = Synthesize(One(Up, Radar::State::Exit));
	Check(single.size() == CueSamples, "one cue lasts exactly the compiled cue duration in stereo frames");
	Radar::Events pair{};
	pair[Up] = Radar::State::Exit;
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
	const auto left = Synthesize(One(Left, Radar::State::Exit));
	const auto right = Synthesize(One(Right, Radar::State::Exit));
	const auto up = Synthesize(One(Up, Radar::State::Exit));
	const auto down = Synthesize(One(Down, Radar::State::Exit));
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

void DiagonalFrame() {
	// The diagonal rays sit in the screen's corners: every one is panned to its
	// side, and the two lower ones take down's pitch, so left/right and up/down
	// stay two independent cues.
	constexpr auto Diagonal = Radar::Frame::Diagonal;
	constexpr size_t Window = CueFrames / 2;
	const double high = Frequency(Synthesize(One(Up, Radar::State::Exit)), 0, 0, Window);
	const double low = Frequency(Synthesize(One(Down, Radar::State::Exit)), 0, 0, Window);
	const auto& rays = Radar::RaysOf(Diagonal);
	for (size_t i = 0; i < Radar::DirectionCount; ++i) {
		const auto pcm = Synthesize(One(i, Radar::State::Exit), {}, 1, Diagonal);
		const size_t loud = rays[i].screen.x < 0 ? 0 : 1;
		Check(Rms(pcm, loud, 0, CueFrames) > 3 * Rms(pcm, 1 - loud, 0, CueFrames),
			"each diagonal ray is louder in the ear of its screen side");
		const auto side = Synthesize(One(rays[i].screen.x < 0 ? Left : Right, Radar::State::Exit));
		Check(std::abs(Rms(pcm, loud, 0, CueFrames) - Rms(side, loud, 0, CueFrames)) < 1e-6 || rays[i].screen.y > 0,
			"an upper diagonal is panned exactly like the side it leans to");
		const double pitch = Frequency(pcm, loud, 0, Window);
		Check(std::abs(pitch - (rays[i].screen.y > 0 ? low : high)) < 1e-9, "a lower diagonal takes down's pitch, an upper one up's");
	}
}

void Register() {
	// Measure over the loud part of the cue, where the decay still leaves a
	// clean waveform in every register.
	constexpr size_t Window = CueFrames / 2;
	const auto openUp = Synthesize(One(Up, Radar::State::Exit));
	const auto blockedUp = Synthesize(One(Up, Radar::State::Blocked));
	const auto openDown = Synthesize(One(Down, Radar::State::Exit));
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
	Check(std::abs(Frequency(Synthesize(One(Left, Radar::State::Exit)), 0, 0, Window) - open) < 1e-9
		&& std::abs(Frequency(Synthesize(One(Right, Radar::State::Exit)), 1, 0, Window) - open) < 1e-9,
		"only down is retuned; the sides keep the opening register");
	// The wall is a shorter, duller tock: it dies away faster than an opening.
	const auto decay = [](const std::vector<int16_t>& pcm) {
		return Rms(pcm, 0, CueFrames / 2, CueFrames) / Rms(pcm, 0, 0, CueFrames / 2);
	};
	Check(decay(blockedUp) < decay(openUp), "the wall tock decays faster than the opening tin");
}

void Readings() {
	const auto exit = Synthesize(One(Up, Radar::State::Exit)), floor = Synthesize(One(Up, Radar::State::Open));
	const auto wall = Synthesize(One(Up, Radar::State::Blocked)), door = Synthesize(One(Up, Radar::State::Door));
	Check(door.size() == CueSamples && floor.size() == BreathSamples, "a struck reading fills one cue slot, the breath its longer one");
	const double breath = Rms(floor, 0, 0, BreathFrames) / Rms(exit, 0, 0, CueFrames);
	Check(breath > 0.35 && breath < 0.8, "floor is a breath under the struck cues, but not buried by the game (2026-09-21 listening)");
	Check(Peak(floor) < Peak(exit), "the breath never strikes harder than the tin");
	Check(Rms(floor, 0, BreathFrames / 4, 3 * BreathFrames / 4) > 2 * Rms(floor, 0, 0, BreathFrames / 8),
		"the breath swells in the middle instead of striking at the start");
	Check(Frequency(door, 0, 0, CueFrames / 8) < Frequency(wall, 0, 0, CueFrames / 8), "the door knock sits below the wall tock");
	// Two strikes: loud, a rest, loud again, where a wall's one strike is still dying away.
	const size_t rest = static_cast<size_t>(0.045 * SampleRate), second = static_cast<size_t>(0.06 * SampleRate);
	const double first = Rms(door, 0, 0, rest / 2), between = Rms(door, 0, rest, second), again = Rms(door, 0, second, second + rest / 2);
	Check(between < first / 5 && again > between * 5, "a door is two knocks with a rest between them");
	Check(Rms(wall, 0, rest, second) > between, "a wall is one strike that has not rested where the door has");
	Check(Frequency(Synthesize(One(Down, Radar::State::Door)), 0, 0, CueFrames / 8) < Frequency(door, 0, 0, CueFrames / 8),
		"a door is lowered for down like every other reading");
	// Two openings at once, up and down, must not blur: up's breath rises, down's
	// falls and the sides hold (the D4 radar's sweep, 2026-09-28 listening).
	constexpr size_t Half = BreathFrames / 2;
	const auto floorDown = Synthesize(One(Down, Radar::State::Open));
	const auto floorLeft = Synthesize(One(Left, Radar::State::Open));
	Check(Frequency(floor, 0, Half, BreathFrames) > 1.3 * Frequency(floor, 0, 0, Half), "up's breath brightens as it plays");
	Check(Frequency(floorDown, 0, Half, BreathFrames) < 0.8 * Frequency(floorDown, 0, 0, Half), "down's breath darkens as it plays");
	const double sideDrift = Frequency(floorLeft, 0, Half, BreathFrames) / Frequency(floorLeft, 0, 0, Half);
	Check(sideDrift > 0.8 && sideDrift < 1.25, "a side's breath holds its colour");
	Check(Frequency(floorDown, 0, 0, BreathFrames) < 0.5 * Frequency(floor, 0, 0, BreathFrames), "down's breath is far darker than up's");
	Check(Peak(floorDown) == Peak(floor), "every breath peaks at the same level");
	bool mirrored = true;
	const auto left = Synthesize(One(Left, Radar::State::Open)), right = Synthesize(One(Right, Radar::State::Open));
	for (size_t frame = 0; frame < BreathFrames; ++frame)
		mirrored &= left[frame * Channels] == right[frame * Channels + 1] && left[frame * Channels + 1] == right[frame * Channels];
	Check(mirrored, "the breath is panned like the struck cues");
	CueQueue urgent;
	Radar::Events mixed{};
	mixed[Up] = Radar::State::Open;
	mixed[Left] = Radar::State::Exit;
	mixed[Right] = Radar::State::Blocked;
	mixed[Down] = Radar::State::Door;
	urgent.Push(mixed);
	Check(urgent.Take()[Right] == Radar::State::Blocked, "a wall warning plays before everything else pending");
	Check(urgent.Take()[Down] == Radar::State::Door, "a door is a warning too, next in the round robin");
	Check(urgent.Take()[Left] == Radar::State::Exit, "an exit plays before plain floor");
	Check(urgent.Take()[Up] == Radar::State::Open && urgent.Empty(), "floor plays last");
}

void Volume() {
	const auto full = Synthesize(One(Up, Radar::State::Exit), 1);
	const auto half = Synthesize(One(Up, Radar::State::Exit), 0.5);
	const auto silent = Synthesize(One(Up, Radar::State::Exit), 0);
	Check(silent.size() == full.size(), "a silenced cue still occupies its slot in the stream");
	Check(std::all_of(silent.begin(), silent.end(), [](int16_t sample) { return sample == 0; }),
		"volume zero produces silence, not a quiet cue");
	Check(Peak(full) > 0 && std::abs(static_cast<double>(Peak(half)) / Peak(full) - 0.5) < 0.01,
		"half volume is half the peak level");
	Check(Peak(full) < 32767, "the compiled peak level never reaches full scale");
	Check(Peak(Synthesize(One(Up, Radar::State::Exit), 5)) == Peak(full),
		"a volume beyond the maximum is clamped instead of distorting");
}

void Queue() {
	CueQueue queue;
	Check(queue.Empty(), "a fresh queue has nothing to play");
	Radar::Events pair{};
	pair[Left] = Radar::State::Exit;
	pair[Down] = Radar::State::Open;
	queue.Push(pair, {.eventMs = 1000, .level = 3});
	Check(!queue.Empty(), "pushed events are pending");
	CueSource source{};
	auto taken = queue.Take(&source);
	Check(taken[Left] == Radar::State::Exit && taken[Down] == Radar::State::Unknown,
		"the worker is handed one direction at a time");
	Check(source.eventMs == 1000 && source.level == 3, "a taken cue carries the source it was pushed with");
	Check(!queue.Empty(), "the second direction stays queued independently");
	taken = queue.Take();
	Check(taken[Down] == Radar::State::Open && taken[Left] == Radar::State::Unknown && queue.Empty(),
		"the queue empties one direction per take");
	CueSource stale{.eventMs = 7};
	taken = queue.Take(&stale);
	Check(std::all_of(taken.begin(), taken.end(), [](Radar::State state) { return state == Radar::State::Unknown; })
		&& stale.eventMs == 0, "taking from an empty queue yields nothing and clears the source");
	Radar::Events two{};
	two[Up] = two[Right] = Radar::State::Exit;
	queue.Push(two);
	Check(queue.Take()[Up] == Radar::State::Exit, "the round robin resumes from the direction after the last one played");
	Radar::Events again{};
	again[Up] = Radar::State::Exit;
	queue.Push(again);
	Check(queue.Take()[Right] == Radar::State::Exit,
		"a direction that just played waits its turn behind one that has been waiting");
	Check(queue.Take()[Up] == Radar::State::Exit && queue.Empty(), "the round robin then comes back round");
	queue.Push(One(Down, Radar::State::Exit), {.eventMs = 10});
	queue.Push(One(Down, Radar::State::Blocked), {.eventMs = 20});
	Check(queue.Pending()[Down] == Radar::State::Blocked && queue.Source(Down).eventMs == 20,
		"a direction keeps only its latest state, with the source that produced it");
	queue.Push(One(Left, Radar::State::Exit));
	Radar::Events confirmed{};
	confirmed[Down] = Radar::State::Blocked;
	confirmed[Left] = Radar::State::Blocked;
	queue.Retain(confirmed, {});
	Check(queue.Pending()[Down] == Radar::State::Blocked && queue.Pending()[Left] == Radar::State::Unknown,
		"a queued cue that no longer describes the confirmed state of its direction is dropped");
	// The unexplored mark is part of what a cue says: a mark that no longer holds drops the cue too.
	Radar::Marks marked{};
	marked[Up] = true;
	queue.Push(One(Up, Radar::State::Exit), {.eventMs = 30}, marked);
	Check(queue.Source(Up).unexplored, "a pushed cue carries its mark in its source");
	Radar::Events exitUp{};
	exitUp[Up] = Radar::State::Exit;
	queue.Retain(exitUp, marked);
	Check(queue.Pending()[Up] == Radar::State::Exit, "the same state with the same mark is kept");
	queue.Retain(exitUp, {});
	Check(queue.Pending()[Up] == Radar::State::Unknown, "the same state without the mark is dropped");
}

// The unexplored tail: the base cue is untouched and two short pips follow it
// after a short gap, on exits and on floor alike, never on walls or doors.
void UnexploredTail() {
	constexpr size_t TailFrames = (Radar::UnexploredMarkGapMs + Radar::UnexploredMarkMs) * SampleRate / 1000;
	constexpr size_t GapFrames = Radar::UnexploredMarkGapMs * SampleRate / 1000;
	constexpr size_t PipFrames = Radar::UnexploredPipMs * SampleRate / 1000;
	constexpr size_t PipGapFrames = Radar::UnexploredPipGapMs * SampleRate / 1000;
	Radar::Marks marked{};
	marked[Up] = true;
	const auto plain = Synthesize(One(Up, Radar::State::Exit));
	const auto tagged = Synthesize(One(Up, Radar::State::Exit), marked);
	Check(tagged.size() == CueSamples + TailFrames * Channels, "a marked exit is the cue plus the tail");
	Check(std::equal(plain.begin(), plain.end(), tagged.begin()), "the base cue is untouched by the mark");
	Check(Rms(tagged, 0, CueFrames, CueFrames + GapFrames / 2) == 0, "a short silence separates the tail");
	const size_t start = CueFrames + GapFrames;
	const size_t second = start + PipFrames + PipGapFrames;
	Check(Rms(tagged, 0, start, start + PipFrames) > 0 && Rms(tagged, 0, second, second + PipFrames) > 0,
		"unexplored sounds two distinct pips");
	Check(Rms(tagged, 0, start + PipFrames, second) == 0, "silence separates the two pips");
	Check(std::equal(tagged.begin() + start * Channels, tagged.begin() + (start + PipFrames) * Channels,
		tagged.begin() + second * Channels), "both pips have the same pitch and loudness, unlike a beacon note ladder");
	const double tail = Frequency(tagged, 0, start, start + PipFrames);
	Check(tail < Frequency(plain, 0, 0, CueFrames / 8), "and it sits below the tin");
	const auto floor = Synthesize(One(Up, Radar::State::Open), marked);
	Check(floor.size() == BreathSamples + TailFrames * Channels, "floor takes the same tail");
	Check(std::equal(floor.begin() + static_cast<long long>(BreathSamples), floor.end(), tagged.begin() + static_cast<long long>(CueSamples)),
		"one tail for both, so it is one thing to learn");
	Check(Synthesize(One(Up, Radar::State::Blocked), marked).size() == CueSamples
		&& Synthesize(One(Up, Radar::State::Door), marked).size() == CueSamples, "walls and doors never carry it");
	Radar::Marks markedDown{};
	markedDown[Down] = true;
	const auto down = Synthesize(One(Down, Radar::State::Exit), markedDown);
	Check(Frequency(down, 0, start, start + PipFrames) < tail
		&& Frequency(down, 0, second, second + PipFrames) < tail, "down lowers both pips like the cue");
}
}
int main() {
	Layout();
	StereoPosition();
	DiagonalFrame();
	Register();
	Readings();
	Volume();
	Queue();
	UnexploredTail();
	Passed();
}
