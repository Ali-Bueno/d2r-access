#pragma once

namespace D2RL {
struct PluginContext;
}

// Some panels are action prompts rather than things you navigate. The
// delete-character modal is the clearest case: in controller mode its two
// buttons carry ButtonLegendWidget children whose presets are the global
// "select" and "back" actions (game-data Controller/ConfirmationModalHD.json),
// so the game never gives anything focus and the player is meant to press a
// button, not move to one.
//
// Both readers in this mod are focus-driven, so such a panel is silent: there
// is no focus to report and nothing to match. The answer is NOT to put focus
// there -- moving the game's own cursor is what broke the controller focus
// handling before. It is to read the panel when it opens, from what it
// actually shows: its prompt and its legends.
namespace D2RAccess::ModalReader {

// Reads a panel that has just become active while nothing holds focus. Cheap:
// it does nothing at all unless the active panel changed.
void Tick(const D2RL::PluginContext* context);

void Reset();

}
