#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::WallSlideService {
void Register(const D2RL::PluginContext* context, bool nativeHooks);
void Tick();
void Unregister();
// Called only when the native movement code attempts to redirect a blocked
// target; `push` is the world direction it tried first, when known. A `retry`
// (the game's 45-degree snap) is let through in a narrow passage along `push`.
bool Prevent(uintptr_t pathContext, const float* push = nullptr, bool retry = false) noexcept;
// From the UI thread: the ways the player threads a passage along (WallBump::Squeezes).
void SetSqueezes(uint8_t ways) noexcept;
// Whether stairs and narrow passages keep the game's slide (a tester, 2026-10-23: stuck on the
// Arcane Sanctuary's stairs), for the keyboard's settings list.
bool NarrowSlide() noexcept;
void SetNarrowSlide(bool on) noexcept;
// The local player's refused stick attempts so far, prevented or not, and the
// direction of the latest: the wall bump's evidence of contact.
struct Blocked { uint64_t attempts = 0; float x = 0, y = 0; };
Blocked LastBlocked() noexcept;
// True while a blocked stick direction stops the character instead of sliding.
bool Enabled() noexcept;
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
void SetEnabled(bool enabled);
}
