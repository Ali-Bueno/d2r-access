#pragma once

#include <D2RLPlugin/api.h>
#include <vector>

// Observes the game's own keyboard skill hotkeys without consuming them. The hooks copy ids only;
// names and speech belong on the UI tick. docs/skill-hotkeys.md
namespace D2RAccess::NativeSkillHotkeys {
struct Event { int slot = -1, skill = -1; bool quickCast = false; };
// Off, with one log line, when a fingerprint or hook fails; needs NativeSkillBar::Initialize first.
bool Initialize(const D2RL::PluginContext* context);
// Asked with the skill as a quick cast key goes down, before the game casts it where the
// pointer is: true when it gave the game a target to hold for that cast (NativeHover::Hold).
// `cast` is told the skill once the game has put it in hand, for what it cannot aim itself.
void SetQuickAim(bool (*aim)(int skill), void (*cast)(int skill));
// Events since the last call, oldest first. UI thread.
std::vector<Event> Take();
}
