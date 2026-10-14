#pragma once

#include "native_ui.h"
#include "ui_text.h"

#include <string>
#include <vector>

// Character and hireling stat panels: their attribute, defence, life,
// mana, stamina and resistance rows. The four character attributes that can be raised
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

// Every statistic the panel is showing, label and value, in the order the
// layout declares them -- which is the order they appear down the screen --
// after the character's name and then its level and class. A
// row whose value has not rendered yet reads as its label alone; a row with
// no label is skipped, as are rows the game has hidden. Empty when the panel
// is not visible.
//
// Each row's value is looked up among that row's own siblings rather than
// anywhere under the panel, because the attack damage and attack rating rows
// exist twice -- once under LeftSkillStats and once under RightSkillStats --
// with identical widget names.
std::vector<std::string> Lines(uintptr_t panel);

// The character's name as the panel shows it, empty until the game has
// rendered it.
std::string Name(uintptr_t panel);

// The stat points left to spend, as the panel says it ("5 Stat Points
// Remaining"); empty when it cannot be read.
std::string Points(uintptr_t panel);

// True when a points line ("5 Stat Points Remaining") names a count above zero.
bool HasPoints(std::string_view points);

// The attribute a "+" button raises (StrengthPointButton: StrengthTitle and
// StrengthStat), read whether or not the button is showing; it hides with the
// last point spent. Empty parts when they cannot be read.
struct Attribute { std::string label, value; };
Attribute AttributeOf(uintptr_t panel, std::string_view button);

void Reset();

}
