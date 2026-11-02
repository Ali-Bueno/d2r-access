#pragma once

#include <cstdint>

namespace D2RL {
struct PluginContext;
}

// An edited text field is the one control whose label tells the player almost
// nothing. "Character Name, text field" does not say what is in the box, and
// while a name is being typed the only thing the player cannot recover is what
// they just removed -- they know what they pressed, but a backspace is silent.
//
// So this echoes typed characters and names the ones a deletion takes away; the
// focus tracker reads the contents with the label when focus arrives.
namespace D2RAccess::TextField {

// Samples the focused widget once per UI update. Cheap: one classify and one
// string read, and only while an editable field actually holds focus.
void Tick(const D2RL::PluginContext* context);

// Drops the tracked field, so re-entering a panel reads its contents again
// rather than treating the first sample as an edit.
void Reset();

// The editable box the game's focus is on, as the last Tick saw it; 0 when none.
uintptr_t EditedWidget();

}
