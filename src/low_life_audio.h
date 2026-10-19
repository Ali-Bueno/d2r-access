#pragma once

#include <D2RLPlugin/api.h>

// The Diablo IV low-life sound (the author's Last Epoch mod's copy): its start once,
// then its heartbeat looping until told to stop. Its own voice and volume
// (SoundVolume::Family::LowLife), on a worker so the game's thread never waits.
namespace D2RAccess::LowLifeAudio {
bool Start(const D2RL::PluginContext* context);
// On plays the start and then loops the heartbeat; off silences it at once.
void Play(bool on);
void Stop();
}
