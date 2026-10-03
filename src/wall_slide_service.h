#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::WallSlideService {
void Register(const D2RL::PluginContext* context, bool nativeHooks);
void Tick();
void Unregister();
// Called only when the native movement code attempts to redirect a blocked target.
bool Prevent(uintptr_t pathContext) noexcept;
// True while a blocked stick direction stops the character instead of sliding.
bool Enabled() noexcept;
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
void SetEnabled(bool enabled);
}
