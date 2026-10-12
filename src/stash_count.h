#pragma once

#include <string>

// The stack count the gem, rune and material stash tabs draw on a slot, after the item's name.
// docs/native-tooltips.md#stash-stack-counts
namespace D2RAccess::StashCount {
// `name, count` when the slot shows a number; the name alone otherwise (the game hides the count of an empty slot).
std::string WithCount(const std::string& name, const std::string& shownText, bool shown, const char* separator);
}
