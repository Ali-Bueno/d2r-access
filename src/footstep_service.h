#pragma once
#include <D2RLPlugin/api.h>

namespace D2RAccess::FootstepService {
void Register(const D2RL::PluginContext* context);
void Tick();
void Unregister();
bool Suppress(uint32_t entity) noexcept;
// A footfall the game played, with its sound; the player's own is scaled by the footstep volume.
void Played(uint32_t entity, uintptr_t sound) noexcept;
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled();
void SetEnabled(bool enabled);
// The player's own footsteps as a percent of the game's, a step of Footsteps::VolumePercents.
int VolumeLevel();
void SetVolumeLevel(int level);
}
