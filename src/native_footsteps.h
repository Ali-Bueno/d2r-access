#pragma once
#include "footsteps.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::NativeFootsteps {
bool Initialize(const D2RL::PluginContext* context);
Footsteps::Sample Read(uint32_t entity) noexcept;
}
