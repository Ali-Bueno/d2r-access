#pragma once
#include "ground_items.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::ItemAudio {
constexpr bool IsOneShot(GroundItems::Sound sound) {
	return sound == GroundItems::Sound::InsideUnexploredRoom || sound == GroundItems::Sound::Discovered;
}
constexpr bool SameSource(const GroundItems::Voice& a, const GroundItems::Voice& b) {
	return a.id == b.id && a.sound == b.sound && a.source == b.source;
}
bool Start(const D2RL::PluginContext* context);
void Update(std::vector<GroundItems::Voice> voices);
void Silence();
void UpdateExploration(std::vector<GroundItems::Voice> voices);
void PlayExplorationArrival(GroundItems::Voice voice);
// One centered cue for a find the radar has just announced; its own voice, apart from exploration.
void PlayDiscovery();
// One sound, centred and played once, for the sound glossary.
void Preview(GroundItems::Sound sound);
void SilenceExploration();
void Stop();
bool Available();
}
