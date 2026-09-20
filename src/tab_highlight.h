#pragma once

#include <D2RLPlugin/api.h>

// Says which tab the highlight is on while the player is parked on a tab
// strip and walking it with the D-pad.
//
// This is not what TabReader does. TabReader announces a tab when the
// SELECTION changes, and it is suspended while a panel owns speech; landing
// on a strip is announced by the focus tracker. Neither covers moving the
// highlight along a strip that already has focus, because the focused widget
// does not change and no panel opens until the player commits.
//
// Narrow on purpose: it runs only while the controller focus is on a tab
// strip itself, which is never true while the player is browsing a stash
// grid. Widening TabReader's suspend rule to reach the same case is what
// doubled up the stash tabs in 8f69593.
namespace D2RAccess::TabHighlight {

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Reset();

}
