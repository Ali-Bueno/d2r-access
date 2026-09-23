#include "entity_cue_audio.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace D2RAccess::EntityCueAudio;

namespace {
int Peak(const std::vector<int16_t>& pcm) {
	int peak = 0;
	for (int16_t sample : pcm) peak = std::max(peak, std::abs(static_cast<int>(sample)));
	return peak;
}

double Rms(const std::vector<int16_t>& pcm, size_t channel) {
	double sum = 0;
	for (size_t frame = 0; frame < pcm.size() / Channels; ++frame) {
		const double sample = pcm[frame * Channels + channel];
		sum += sample * sample;
	}
	return std::sqrt(sum / static_cast<double>(pcm.size() / Channels));
}

int Crossings(const std::vector<int16_t>& pcm, size_t fromFrame, size_t toFrame) {
	int crossings = 0, prior = 0;
	for (size_t frame = fromFrame; frame < toFrame; ++frame) {
		const int16_t sample = pcm[frame * Channels];
		const int sign = (sample > 0) - (sample < 0);
		if (sign && prior && sign != prior) ++crossings;
		if (sign) prior = sign;
	}
	return crossings;
}

void DurationAndLevels() {
	for (Sound sound : {Sound::Npc, Sound::Important, Sound::Resurrector, Sound::Ranged}) {
		const auto pcm = Synthesize(sound, {});
		Check(!pcm.empty() && pcm.size() % Channels == 0, "every cue contains interleaved stereo PCM");
		Check(pcm.size() / Channels <= SampleRate * 120 / 1000, "every cue is at most 120 ms");
		Check(Peak(pcm) > 1500 && Peak(pcm) < 14000, "cue is audible with headroom");
		Check(pcm.front() == 0 && pcm.back() == 0, "cue starts and ends smoothly at silence");
	}
}

void Spatial() {
	const auto left = Synthesize(Sound::Npc, {-0.75, 1});
	const auto right = Synthesize(Sound::Npc, {0.75, 1});
	Check(Rms(left, 0) > 2 * Rms(left, 1), "left placement favors left ear");
	Check(Rms(right, 1) > 2 * Rms(right, 0), "right placement favors right ear");
	bool mirror = left.size() == right.size();
	for (size_t i = 0; i + 1 < left.size(); i += Channels)
		mirror &= left[i] == right[i + 1] && left[i + 1] == right[i];
	Check(mirror, "left and right placements mirror exactly");
	const auto centered = Synthesize(Sound::Npc, {});
	bool equalChannels = true;
	for (size_t i = 0; i + 1 < centered.size(); i += Channels)
		equalChannels &= centered[i] == centered[i + 1];
	Check(equalChannels, "front center plays equally in both ears");
	const auto rear = Synthesize(Sound::Npc, {0, -1});
	constexpr size_t FirstNoteFrames = 46 * SampleRate / 1000;
	Check(Crossings(rear, 100, FirstNoteFrames - 100) < Crossings(centered, 100, FirstNoteFrames - 100),
		"rear placement lowers the first note by a meaningful interval");
}

void Timbres() {
	const auto npc = Synthesize(Sound::Npc, {});
	const auto important = Synthesize(Sound::Important, {});
	const auto resurrector = Synthesize(Sound::Resurrector, {});
	const auto ranged = Synthesize(Sound::Ranged, {});
	Check(npc != important, "friendly and enemy signatures differ");
	Check(important != resurrector && important != ranged && resurrector != ranged,
		"important, resurrector and ranged signatures differ");
	constexpr size_t FirstNoteFrames = 46 * SampleRate / 1000;
	constexpr size_t SecondNoteBegin = 54 * SampleRate / 1000;
	Check(Crossings(npc, SecondNoteBegin + 100, SecondNoteBegin + FirstNoteFrames - 100)
		> Crossings(npc, 100, FirstNoteFrames - 100), "NPC cue rises to a second note");
	Check(Crossings(important, 100, FirstNoteFrames - 100) < Crossings(npc, 100, FirstNoteFrames - 100),
		"enemy cue uses a lower register than the NPC cue");
}

void InvalidInputsAndGain() {
	const auto full = Synthesize(Sound::Important, {});
	const auto half = Synthesize(Sound::Important, {}, 0.5);
	bool proportional = true;
	for (size_t i = 0; i < full.size(); ++i)
		proportional &= std::abs(static_cast<int>(half[i]) * 2 - full[i]) <= 2;
	Check(proportional, "gain scales samples proportionally");
	const auto muted = Synthesize(Sound::Important, {}, 0);
	Check(Peak(muted) == 0, "zero gain is silent");
	Check(Peak(Synthesize(Sound::Important, {}, -1)) == 0, "negative gain is silent");
	Check(Peak(Synthesize(Sound::Important, {}, std::numeric_limits<double>::infinity())) == 0,
		"nonfinite gain is silent");
	Check(Synthesize(Sound::Npc, {std::numeric_limits<double>::quiet_NaN(),
		std::numeric_limits<double>::infinity()}) == Synthesize(Sound::Npc, {}),
		"nonfinite coordinates fall back to center front");
	Check(Synthesize(Sound::Npc, {10, -10}) == Synthesize(Sound::Npc, {1, -1}),
		"coordinates beyond the unit range are clamped");
}
}

int main() {
	DurationAndLevels();
	Spatial();
	Timbres();
	InvalidInputsAndGain();
	Passed();
}
