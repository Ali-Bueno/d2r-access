#pragma once
#include "attack_reach.h"
#include <vector>
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
// The LT bar of the current weapon set in SkillButton order, or with `main` the
// bar shown without LT. The name is the game's own, "" when its table row
// cannot be read. UI thread only.
std::optional<std::array<Binding, Buttons>> AlternateBar(bool main = false);

// The game's condition for drawing the LT bar on the HUD (0x2399F0).
bool BackBarShown() noexcept;

// A skill's name in the game's language, by skill id; "" when its table row
// cannot be read or it has no description. UI thread only.
std::string SkillName(int skill);
std::optional<std::vector<AttackReach::Skill>> BoundSkills();
std::string ClassSkillName(int page, int row, int column);
}
