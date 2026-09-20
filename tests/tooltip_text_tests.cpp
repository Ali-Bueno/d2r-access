#include "tooltip_text.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace D2RAccess;
namespace {
// Font-size commands are U+E07E followed by '1' or '2'; '2' is the title.
constexpr const char* Title = "\xEE\x81\xBE" "2";
const std::vector<std::string> BaseStats{"One-Hand Damage:", "Defense:"};

std::string TopDown() {
	return std::string(Title) + "Bloodletter\nShort Sword\nOne-Hand Damage: 7 to 15\n"
		"Required Strength: 25\n+15 to Attack Rating\nSell price: 84 gold";
}
// The same tooltip if the engine stores its lines the other way up.
std::string BottomUp() {
	return std::string("Sell price: 84 gold\n+15 to Attack Rating\nRequired Strength: 25\n"
		"One-Hand Damage: 7 to 15\nShort Sword\n") + Title + "Bloodletter";
}
}

int main() {
	for (const auto& [name, text] : std::vector<std::pair<const char*, std::string>>{
			 {"stored top down", TopDown()}, {"stored bottom up", BottomUp()}}) {
		const auto body = TooltipText::Read(text);
		Check(body.lines.size() == 6, "every drawn line survives");
		Check(body.Marked(), "the renderer's title styling is found");
		const auto split = TooltipText::Split(body, BaseStats);
		Check(split.headline == "Bloodletter. One-Hand Damage: 7 to 15",
			std::string("the name leads and the base damage follows it, ") + name);
		Check(split.detail.find("Required Strength: 25") != std::string::npos, "requirements are detail");
		Check(split.detail.find("+15 to Attack Rating") != std::string::npos, "properties are detail");
		Check(split.detail.find("Sell price: 84 gold") != std::string::npos, "the price is detail");
		Check(split.detail.find("Bloodletter") == std::string::npos, "the name is not repeated in the detail");
		Check(split.detail.find("One-Hand Damage") == std::string::npos, "nor is the base damage");
	}

	// Detail keeps the order it was stored in, so it reads as the screen drew it.
	const auto ordered = TooltipText::Split(TooltipText::Read(TopDown()), BaseStats);
	Check(ordered.detail == "Short Sword. Required Strength: 25. +15 to Attack Rating. Sell price: 84 gold",
		"the detail is the rest of the tooltip in order");

	// Armour reads the same way through its own label.
	const auto armour = TooltipText::Split(TooltipText::Read("Quilted Armor\nDefense: 43\nRequired Strength: 20"), BaseStats);
	Check(armour.headline == "Quilted Armor. Defense: 43", "defense is a base stat too");
	Check(armour.detail == "Required Strength: 20", "and the requirement is not");

	// No styling: the first line stands in, and the split says so.
	const auto plain = TooltipText::Read("Minor Healing Potion\nRestores Health");
	Check(!plain.Marked(), "an unstyled tooltip reports that it was unstyled");
	const auto potion = TooltipText::Split(plain, BaseStats);
	Check(potion.headline == "Minor Healing Potion", "an item with no base damage is just its name");
	Check(potion.detail == "Restores Health", "the rest is still offered");

	// Without the game's labels the headline is the name alone: a shorter
	// readout, never a wrong one.
	const auto unlabelled = TooltipText::Split(TooltipText::Read(TopDown()), {});
	Check(unlabelled.headline == "Bloodletter", "no labels, no base-stat line in the headline");
	Check(unlabelled.detail.find("One-Hand Damage: 7 to 15") != std::string::npos, "and nothing is lost -- it moves to the detail");

	Check(TooltipText::Read("").lines.empty(), "an empty tooltip has no lines");
	Check(TooltipText::Split(TooltipText::Read(""), BaseStats).Empty(), "and nothing to say");
	Check(TooltipText::Read("\n\n\n").lines.empty(), "blank lines are not content");
	Passed();
}
