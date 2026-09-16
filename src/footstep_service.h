#pragma once
#include <D2RLPlugin/api.h>

namespace D2RAccess::FootstepService {
void Register(const D2RL::PluginContext* context);
void Tick();
void Unregister();
bool Suppress(uint32_t entity) noexcept;
}
