#pragma once

#include "radar.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::RadarAudio {
// Small PCM cues, synthesized on the audio thread. Windows' built-in mixer;
// no new native library or loose sound files.
inline constexpr unsigned SampleRate = 48000, Channels = 2;
inline constexpr unsigned CueGapMs = static_cast<unsigned>(Radar::CueGapMs);
// Bounded, latest-state queue. The worker plays one direction at a time;
// observations invalidate queued cues before the player hears stale openings.
class CueQueue {
public:
	void Push(const Radar::Events& events);
	void Retain(const Radar::Events& observations);
	Radar::Events Take();
	bool Empty() const;
private:
	Radar::Events pending_{};
	size_t next_ = 0;
};
std::vector<int16_t> Synthesize(const Radar::Events& events);
bool Start(const D2RL::PluginContext* context);
void Play(const Radar::Events& events);
void Validate(const Radar::Events& observations, const Radar::Events& audible);
void Silence();
void Stop();
bool Available();
}
