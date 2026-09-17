#pragma once

#include "native_ui.h"
#include "ui_text.h"

// The character panel (CharacterStatsPanel): its attribute, defence, life,
// mana, stamina and resistance rows. The four attributes that can be raised
// are focusable buttons and belong to NativeProgression; every other row is a
// plain label whose controller layout adds a focusable "Tooltip" child, so the
// game stops on it and nothing spoke it until now.
namespace D2RAccess::CharacterStats {

// True when the focused widget is one of those label rows.
bool HandlesFocus(NativeUi::Focus focus, const NativeUi::Type& type);

// Label and value of the focused row, with the panel name and the character's
// identity as the body so both are spoken once on entering the panel. Empty
// when the focus is not a readable row.
UiText::Readout Read(NativeUi::Focus focus, const NativeUi::Type& type);

void Reset();

}
