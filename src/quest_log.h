#pragma once

#include "generated/quest_catalog.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

// The quest log's identity layer: which quest a quest-log button stands for,
// and that quest's text in the player's language.
//
// Quest buttons carry an icon but no caption, so without this the screen reader
// lands on a nameless control. The act/quest pair behind each button comes from
// the game's own layout (the button's icon filename) and the text from the
// game's own string table, so nothing here is translated or invented by us.
//
// String-table keys are resolved through a caller-supplied Resolver -- normally
// Localization::Text -- which keeps this layer free of the SDK and testable.
namespace D2RAccess::QuestLog {

using Resolver = std::function<std::string(std::string_view key)>;

// The quest a quest-log button stands for, or nullptr when the widget is not
// one (the catalog is keyed by the button names the game ships).
const Generated::Quest* ForWidget(std::string_view widget);

// The quest's name, empty when the game's tables cannot resolve it.
std::string Title(const Generated::Quest& quest, const Resolver& resolve);

// The quest's objective lines, in the order the game ships them. Blank lines
// are dropped. Lines carrying a printf placeholder are the live counters
// ("Monsters left: %d") and are left out until a value is known -- speaking the
// raw placeholder would be worse than silence.
std::vector<std::string> Objectives(const Generated::Quest& quest, const Resolver& resolve);

// True when a string-table line expects a runtime value, so callers can tell a
// plain objective from a counter that still needs one.
bool NeedsValue(std::string_view text);

// The counter line filled in with its value ("Monsters left: 3"), or an empty
// string when this quest ships no such line. D2 keeps a separate singular line
// for the last one remaining, which is used when value is 1 and it exists.
std::string Counter(const Generated::Quest& quest, int remaining, const Resolver& resolve);

}
