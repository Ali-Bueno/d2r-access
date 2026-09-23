#pragma once
#include "ground_items.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::ItemAudio {
constexpr bool IsOneShot(GroundItems::Sound sound) { return sound == GroundItems::Sound::InsideUnexploredRoom; }
bool Start(const D2RL::PluginContext* context);
void Update(std::vector<GroundItems::Voice> voices);
void Silence();
void UpdateExploration(std::vector<GroundItems::Voice> voices);
void PlayExplorationArrival(GroundItems::Voice voice);
void SilenceExploration();
void Stop();
bool Available();
}
