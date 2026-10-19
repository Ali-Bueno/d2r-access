#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The stack count the gem, rune and material stash tabs draw on a slot, after the item's name.
// docs/native-tooltips.md#stash-stack-counts
namespace D2RAccess::StashCount {
// Which stash tab the open panel shows, for the gold box that follows it (BankExpansionLayoutHD.json: BankTabs).
enum class GoldTab { Unknown, Personal, Shared };
// The tab whose localized label (ui.json "personal" / "shared") is among the bars' selected labels.
GoldTab TabOf(const std::vector<std::string>& selected, const std::string& personal, const std::string& shared);

// `name, count` when the slot shows a number; the name alone while it shows none.
std::string WithCount(const std::string& name, const std::string& shownText, bool shown, const char* separator);
// The name `emptyFormat` gives an item with none in the stash.
std::string Empty(const std::string& name, const char* emptyFormat);
// `format` (count, name) for a group of identical items, "3 x Chipped Topaz"; the bare name for one.
std::string Counted(const std::string& name, int count, const char* format);
// The item table code a slot widget's layout name is (BankExpansionLayoutHD.json: "gcw", "r01"), 0 for any other name.
uint32_t CodeOfWidgetName(std::string_view name);
}
