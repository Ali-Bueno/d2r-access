#pragma once
#include "ground_items.h"
#include "item_wave.h"
#include "percent_steps.h"
#include <D2RLPlugin/api.h>
#include <array>
#include <cstdint>

namespace D2RAccess::ItemAudio {
// A drop sound repeats once a second at 100 percent speed; faster cuts a long one
// short with a brief fade, slower leaves more silence (a tester, 2026-10-13).
inline constexpr auto DropSpeedPercents = PercentSteps::Quarters<50, 300>();
inline constexpr int NeutralDropSpeedLevel = PercentSteps::LevelOf(DropSpeedPercents, 100);
inline constexpr uint32_t DropPassMs = 1000, DropFadeMs = 10;
static_assert(DropSpeedPercents[NeutralDropSpeedLevel] == 100);
// One pass of a drop sound's loop, in samples, at a speed level.
constexpr size_t DropPassSamples(int level) {
	return size_t{ItemWave::SampleRate} * DropPassMs * 100 / static_cast<size_t>(DropSpeedPercents[static_cast<size_t>(level)]) / 1000;
}
int DropSpeedLevel();
void SetDropSpeedLevel(int level);
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
