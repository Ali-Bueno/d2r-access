#pragma once
#include "footsteps.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::NativeFootsteps {
bool Initialize(const D2RL::PluginContext* context);
Footsteps::Sample Read(uint32_t entity) noexcept;
// A playing sound's volume, 0-255 as the game sets it; -1 when unreadable.
bool VolumeSupported();
int Volume(uintptr_t handle) noexcept;
void SetVolume(uintptr_t handle, int volume) noexcept;
}
