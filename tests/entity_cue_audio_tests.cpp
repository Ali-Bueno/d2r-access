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

double Rms(const std::vector<int16_t>& pcm, size_t channel, size_t beginFrame = 0, size_t endFrame = 0) {
	if (!endFrame) endFrame = pcm.size() / Channels;
	double sum = 0;
	for (size_t frame = beginFrame; frame < endFrame; ++frame) {
		const double sample = pcm[frame * Channels + channel];
		sum += sample * sample;
	}
	return std::sqrt(sum / static_cast<double>(endFrame - beginFrame));
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
		const size_t frames = pcm.size() / Channels;
		if (sound == Sound::Npc) {
			Check(Rms(pcm, 0, frames * 2 / 3, frames) < 0.25 * Rms(pcm, 0, 0, frames / 3),
				"the NPC retains its quickly decaying wooden tap");
		} else {
			const double middle = Rms(pcm, 0, frames / 3, frames * 2 / 3);
			Check(middle > 2 * Rms(pcm, 0, 0, frames / 3) && middle > 2 * Rms(pcm, 0, frames * 2 / 3, frames),
				"enemy air swells and fades instead of striking sharply");
		}
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
	constexpr size_t BodyFrames = 32 * SampleRate / 1000;
	Check(Crossings(rear, 100, BodyFrames) < Crossings(centered, 100, BodyFrames),
		"rear placement lowers the tap's resonance by a meaningful interval");
}

void Timbres() {
	const auto npc = Synthesize(Sound::Npc, {});
	const auto important = Synthesize(Sound::Important, {});
	const auto resurrector = Synthesize(Sound::Resurrector, {});
	const auto ranged = Synthesize(Sound::Ranged, {});
	Check(npc != important, "friendly and enemy signatures differ");
	Check(important != resurrector && important != ranged && resurrector != ranged,
		"important, resurrector and ranged signatures differ");
	for (Sound sound : {Sound::Important, Sound::Resurrector, Sound::Ranged}) {
		const auto sweep = Synthesize(sound, {});
		const size_t frames = sweep.size() / Channels;
		Check(Crossings(sweep, frames / 6, frames / 3) > Crossings(sweep, frames * 2 / 3, frames * 5 / 6),
			"enemy whoosh sweeps down in brightness");
		const auto rear = Synthesize(sound, {0, -1});
		Check(Crossings(rear, frames / 3, frames * 2 / 3) < Crossings(sweep, frames / 3, frames * 2 / 3),
			"rear whoosh remains audibly lower than front");
	}
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
