#include "stash_count.h"
#include "test_support.h"
#include <D2RLPlugin/item.h>

using D2RAccess::StashCount::WithCount;

int main() {
	constexpr const char* Empty = "%s: 0";
	Check(WithCount("Chipped Topaz", "5", true, ", ") == "Chipped Topaz, 5", "the count follows the name");
	Check(WithCount("El Rune", "99", true, ", ") == "El Rune, 99", "up to the stack limit");
	Check(WithCount("Chipped Topaz", "", false, ", ") == "Chipped Topaz", "an item whose count is not drawn yet keeps its name alone");
	Check(WithCount("Chipped Topaz", "", true, ", ") == "Chipped Topaz", "an unreadable count is left out");
	Check(WithCount("Chipped Topaz", "x5", true, ", ") == "Chipped Topaz", "text that is not a number is left out");
	Check(D2RAccess::StashCount::Empty("Chipped Amethyst", Empty) == "Chipped Amethyst: 0", "an empty slot is named with a zero count");
	using D2RAccess::StashCount::CodeOfWidgetName;
	Check(CodeOfWidgetName("gcw") == D2RL::Items::MakeItemCode("gcw"), "a three-letter slot name is an item code");
	Check(CodeOfWidgetName("r01") == D2RL::Items::MakeItemCode("r01"), "as is a rune's");
	Check(CodeOfWidgetName("") == 0 && CodeOfWidgetName("advancedstash_gems") == 0 && CodeOfWidgetName("a b") == 0,
		"names that are not codes map to nothing");
	using D2RAccess::StashCount::GoldTab;
	using D2RAccess::StashCount::TabOf;
	Check(TabOf({"Personal"}, "Personal", "Shared") == GoldTab::Personal, "the personal tab");
	Check(TabOf({"Gems", "shared"}, "Personal", "Shared") == GoldTab::Shared, "the shared tab, whichever bar holds it and however it is cased");
	Check(TabOf({"Gems"}, "Personal", "Shared") == GoldTab::Unknown && TabOf({}, "Personal", "Shared") == GoldTab::Unknown,
		"any other tab, or none, is unknown");
	Check(TabOf({""}, "", "") == GoldTab::Unknown, "unlocalized labels never match an empty one");
	Passed();
}
