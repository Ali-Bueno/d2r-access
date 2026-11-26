#include "binding_store.h"
#include "test_support.h"

using namespace D2RAccess::BindingStore;

namespace {

// The shape D2RLoader writes (measured 2026-11-26), with a custom default that must not count.
constexpr std::string_view File = "schema_version = 1\r\n"
	"\r\n"
	"[bindings.\"d2r-access/keyboard-object-next\"]\r\n"
	"primary_key = 186\r\n"
	"primary_modifier = 0\r\n"
	"secondary_key = 0\r\n"
	"secondary_modifier = 0\r\n"
	"has_custom_defaults = true\r\n"
	"custom_primary_key = 190\r\n"
	"custom_primary_modifier = 1\r\n"
	"\r\n"
	"[bindings.\"d2rloader/console\"]\r\n"
	"primary_key = 192\r\n"
	"primary_modifier = 2\r\n"
	"\r\n"
	"[bindings.\"d2r-access/keyboard-npc-next\"]\r\n"
	"primary_key = 221\r\n"
	"primary_modifier = 0\r\n"
	"secondary_key = 222\r\n"
	"secondary_modifier = 3\r\n";

void ParsesOnlyThisPlugin() {
	const auto saved = Parse(File, "d2r-access");
	Check(saved.size() == 2 && !saved.contains("console"), "another plugin's keys are left out");
	Check(saved.at("keyboard-object-next") == Keys{186, 0, 0, 0}, "the saved key, not the custom default");
	Check(saved.at("keyboard-npc-next") == Keys{221, 0, 222, 3}, "both slots are read");
	Check(Parse("", "d2r-access").empty() && Parse("[bindings.\"d2r-access/x\"\nprimary_key = 5", "d2r-access").empty(),
		"an empty or broken file names nothing");
}

void CountsWhatTheNextStartWouldLose() {
	const auto saved = Parse(File, "d2r-access");
	const Keys period{190, 1, 0, 0};
	Check(Unsaved({{"keyboard-object-next", period, {186, 0, 0, 0}}}, saved) == 0, "a key that was applied is saved");
	Check(Unsaved({{"keyboard-object-next", period, {222, 0, 0, 0}}}, saved) == 1, "a key changed after Apply is not");
	Check(Unsaved({{"keyboard-item-next", {190, 0, 0, 0}, {190, 0, 0, 0}}}, saved) == 0, "an action the file does not name keeps its default");
	Check(Unsaved({{"keyboard-item-next", {190, 0, 0, 0}, {186, 0, 0, 0}}}, saved) == 1, "one moved off its default without Apply is lost");
}

}

int main() {
	ParsesOnlyThisPlugin();
	CountsWhatTheNextStartWouldLose();
	Passed();
	return 0;
}
