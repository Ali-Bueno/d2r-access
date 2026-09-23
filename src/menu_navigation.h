#pragma once
#include <D2RLPlugin/api.h>
#include <string_view>

namespace D2RAccess::MenuNavigation {
void Initialize(const D2RL::PluginContext* context);
// True while this menu owns the shared cursor adapter instead of the skill grid.
bool Tick(const D2RL::PluginContext* context);
void NoteAction(std::string_view target, std::string_view command);
void Reset();
}
