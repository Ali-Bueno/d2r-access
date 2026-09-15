#pragma once

#include "radar.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::RadarAudio {
// Small PCM cues, synthesized on the audio thread. Windows' built-in mixer;
// no new native library or loose sound files.
inline constexpr unsigned SampleRate = 48000, Channels = 2;
std::vector<int16_t> Synthesize(const Radar::Events& events);
bool Start(const D2RL::PluginContext* context);
void Play(const Radar::Events& events);
void Silence();
void Stop();
bool Available();
}
