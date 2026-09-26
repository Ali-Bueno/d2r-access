#pragma once
#include <D2RLPlugin/api.h>
#include <optional>

// The active weapon set of D2R 3.3.93787, read-only. See docs/weapon-swap.md.
namespace D2RAccess::NativeWeaponSet {
bool Initialize(const D2RL::PluginContext* context);
// True on weapon set II, false on set I, nothing when unreadable. UI thread only.
std::optional<bool> SecondSet() noexcept;
}
