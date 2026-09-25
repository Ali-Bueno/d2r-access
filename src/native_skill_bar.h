#pragma once
#include <D2RLPlugin/api.h>
#include <array>
#include <optional>
#include <string>

// The controller skill bindings of D2R 3.3.93787, read-only. See docs/skill-bar.md.
namespace D2RAccess::NativeSkillBar {
// HUD SkillButton1..6; the LT bar is the next six records of the same table.
inline constexpr int Buttons = 6;

bool Initialize(const D2RL::PluginContext* context);

struct Binding { bool bound = false; int skill = -1; std::string name; };
// The LT bar of the current weapon set in SkillButton order. The name is the
// game's own, "" when its table row cannot be read. UI thread only.
std::optional<std::array<Binding, Buttons>> AlternateBar();

// The game's condition for drawing the LT bar on the HUD (0x2399F0).
bool BackBarShown() noexcept;
}
