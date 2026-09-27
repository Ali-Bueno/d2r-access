#pragma once

#include "speech_strings.h"

#include <string>
#include <string_view>

// In mouse mode the game writes what a click does into an item's tooltip:
// "Ctrl + Left Click to Sell", "Right Click to Use". From the keyboard the
// left click is A and the right click is D (keyboard_menu.h), so while the
// keyboard has the menu the player is told the key to press and not the mouse
// button: "Ctrl + A to Sell", "D to Use". The words for the two clicks are the
// game's own, from ui.json, in each of its languages. docs/keyboard-play.md
namespace D2RAccess::ClickWords {
// `text` with every naming of a click replaced by its key. Pure.
std::string WithKeys(std::string_view text, Strings::Language language);
// The same in the selected language, while the keyboard has the menu; `text`
// as it is otherwise.
std::string Spoken(std::string_view text);
// Whether the keyboard has the menu. Set by the menu's cursor.
void SetKeyboard(bool keyboard);
}
