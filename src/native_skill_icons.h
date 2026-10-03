#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <optional>
#include <string>

// The icons that stand for a skill and carry no text of their own: a skill of
// the skill tree, and a skill of the list of skills for a hand. Each holds the
// id of its skill, and the skill's name is the game's own (native_skill_bar.h).
// docs/keyboard-play.md
namespace D2RAccess::NativeSkillIcons {
bool Initialize(const D2RL::PluginContext* context);

// A skill of the skill tree: a button in a tab's container of skills.
bool InTree(uintptr_t widget);
// A skill of the list of skills: one of the icons the list makes as it opens.
bool InList(uintptr_t widget);
// The id of the skill the icon stands for; empty for any other widget.
std::optional<int> Skill(uintptr_t widget);
// Its name in the game's language; "" for any other widget.
std::string Name(uintptr_t widget);
}
