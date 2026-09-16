#pragma once
#include <D2RLPlugin/api.h>

namespace D2RAccess::SkillNavigation {
void Tick(const D2RL::PluginContext* context);
void SetEnabled(bool enabled);
void Reset();
}
