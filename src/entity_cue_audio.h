#pragma once

#include <D2RLPlugin/api.h>
#include <cstdint>
#include <vector>

namespace D2RAccess::EntityCueAudio {
inline constexpr unsigned SampleRate = 48000, Channels = 2;

enum class Sound { Npc, Important, Resurrector, Ranged, Ordinary, Reach };

// Screen coordinates: -1 right is left, +1 right is right; +1 front is
// ahead and -1 front is behind. Rear cues use a lower pitch.
struct Placement { double right = 0, front = 1; };

std::vector<int16_t> Synthesize(Sound sound, Placement placement, double gain = 1);
bool Start(const D2RL::PluginContext* context, double gain);
void Play(Sound sound, Placement placement);
void Silence();
void Stop();
bool Available();
}
