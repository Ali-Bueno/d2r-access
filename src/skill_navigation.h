#pragma once
#include <D2RLPlugin/api.h>
#include "skill_grid.h"
#include <vector>

namespace D2RAccess::SkillNavigation {
void Tick(const D2RL::PluginContext* context);
void SetEnabled(bool enabled);
void Reset();
// The skills shown in the skill tree panel, each with its column and row.
std::vector<SkillGrid::Node> Nodes(uintptr_t panel);
}
