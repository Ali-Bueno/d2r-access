#pragma once
#include <D2RLPlugin/api.h>

// Says the skill's name when a keyboard skill hotkey (F1 to F8, quick cast) selects another skill.
// `access-skill-hotkeys on|off` toggles it. docs/skill-hotkeys.md
namespace D2RAccess::SkillHotkeyService {
// Installs the observers when `nativeHooks`; false when they are unavailable.
bool Register(const D2RL::PluginContext* context, bool nativeHooks);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
// For the keyboard's settings list (settings_list.h).
bool Enabled();
void SetEnabled(bool enabled);
}
