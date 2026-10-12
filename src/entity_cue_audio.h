#pragma once

#include <D2RLPlugin/api.h>
#include <cstdint>
#include <vector>

namespace D2RAccess::EntityCueAudio {
inline constexpr unsigned SampleRate = 48000, Channels = 2;

// Pickup and Edge are feedback, not presences: a ground item in pickup range, an
// inventory cursor pushed against the grid's edge.
enum class Sound { Npc, Important, Resurrector, Ranged, Ordinary, Reach, Pickup, Edge };

// Screen coordinates: -1 right is left, +1 right is right; +1 front is
// ahead and -1 front is behind. Rear cues use a lower pitch.
// panScale and pitchScale (1 = compiled behavior) widen or narrow the stereo
// spread and the rear pitch drop; the audio layer fills them from the settings.
struct Placement { double right = 0, front = 1, panScale = 1, pitchScale = 1; };

std::vector<int16_t> Synthesize(Sound sound, Placement placement, double gain = 1);
bool Start(const D2RL::PluginContext* context, double gain, double panScale = 1, double pitchScale = 1);
// The settings list changes the strengths while playing.
void SetStrength(double panScale, double pitchScale);
double PanScale();
double PitchScale();
// A pending Reach, Pickup or Edge cue is never replaced by a presence pulse.
void Play(Sound sound, Placement placement);
void Silence();
void Stop();
bool Available();
}
