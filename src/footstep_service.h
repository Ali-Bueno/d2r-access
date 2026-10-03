#pragma once
#include <D2RLPlugin/api.h>

namespace D2RAccess::FootstepService {
void Register(const D2RL::PluginContext* context);
void Tick();
void Unregister();
bool Suppress(uint32_t entity) noexcept;
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled();
void SetEnabled(bool enabled);
}
