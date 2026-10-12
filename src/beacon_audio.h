#pragma once

#include "beacon.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::BeaconAudio {
// The beacon owns its own small stereo output so its pings never queue behind
// the sonar's wall/opening cues: the two speak about different things and must
// be able to overlap. Sounds are synthesized, so no new asset ships.
inline constexpr unsigned SampleRate = 48000, Channels = 2;

enum class Sound {
	Ping,    // the repeating beacon, panned and pitched toward the target
	Locked,  // target acquired: two rising notes
	Cleared, // tracking cancelled: two falling notes
	Arrived, // the target is reached
};
// right: -1 fully left, +1 fully right. front: +1 ahead, -1 behind, matching
// the ground-item loops' screen convention and their rear pitch.
// panScale and pitchScale (1 = compiled behavior) scale the stereo spread and
// the rear pitch drop; the audio layer fills them from the settings.
struct Placement { double right = 0, front = 1, panScale = 1, pitchScale = 1; };

std::vector<int16_t> Synthesize(Sound sound, Placement placement, double gain = 1);
bool Start(const D2RL::PluginContext* context, double gain, double panScale = 1, double pitchScale = 1);
// Queues one sound. The newest ping replaces a ping that has not started yet,
// so a slow device can never build a backlog of stale beacons.
void Play(Sound sound, Placement placement);
void Silence(const char* reason = "suspended");
void Stop();
bool Available();
}
