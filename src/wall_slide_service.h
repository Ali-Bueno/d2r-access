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
}
