#include "tooltip_text.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace D2RAccess;
namespace {
// The renderer's font command is U+E07E followed by one index character,
// '0' through 'V'; index 2 is the one a title is drawn in.
constexpr const char* Title = "\xEE\x81\xBE" "2";
const std::vector<std::string> BaseStats{"One-Hand Damage:", "Defense:"};
const std::string Font0 = std::string(Title).substr(0, 3) + "0";

// The engine stores a body bottom-up -- its first line is the bottom of the
// drawn tooltip and its last is the title. Measured 2026-09-20 from the live
// readouts of a Short Sword and a Minor Healing Potion.
std::string Stored() {
	return std::string("Sell price: 84 gold\n+15 to Attack Rating\nRequired Strength: 25\n"
		"One-Hand Damage: 7 to 15\nShort Sword\n") + Title + "Bloodletter";
}
// The same tooltip the other way up: the title styling finds the name either way.
std::string Inverted() {
	return std::string(Title) + "Bloodletter\nShort Sword\nOne-Hand Damage: 7 to 15\n"
		"Required Strength: 25\n+15 to Attack Rating\nSell price: 84 gold";
}
}

int main() {
	for (const auto& [name, text] : std::vector<std::pair<const char*, std::string>>{
			 {"stored bottom up", Stored()}, {"stored the other way up", Inverted()}}) {
		const auto body = TooltipText::Read(text);
		Check(body.lines.size() == 6, "every drawn line survives");
		Check(body.Marked(), "the renderer's title styling is found");
		const auto split = TooltipText::Split(body, BaseStats);
		Check(split.headline == "Bloodletter", std::string("the name is the headline, ") + name);
		Check(split.detail.find("One-Hand Damage: 7 to 15") != std::string::npos, "the base damage is detail");
		Check(split.detail.find("Required Strength: 25") != std::string::npos, "requirements are detail");
		Check(split.detail.find("+15 to Attack Rating") != std::string::npos, "properties are detail");
		Check(split.detail.find("Sell price: 84 gold") != std::string::npos, "the price is detail");
		Check(split.detail.find("Bloodletter") == std::string::npos, "the name is not repeated in the detail");
	}

	// Detail keeps the order it was stored in, so it reads as the screen drew it.
	const auto ordered = TooltipText::Split(TooltipText::Read(Stored()), BaseStats);
	Check(ordered.detail == "Short Sword. One-Hand Damage: 7 to 15. Required Strength: 25. +15 to Attack Rating. Sell price: 84 gold",
		"the detail is the rest of the tooltip in order");

	// Armour reads the same way through its own label.
	const auto armour = TooltipText::Split(TooltipText::Read("Required Strength: 20\nDefense: 43\nQuilted Armor"), BaseStats);
	Check(armour.headline == "Quilted Armor", "armour is named alone too");
	Check(armour.detail == "Defense: 43. Required Strength: 20", "and its defense leads the detail, where the screen draws it");

	// No styling: the first line stands in, and the split says so.
	const auto plain = TooltipText::Read("Restores Health\nMinor Healing Potion");
	Check(!plain.Marked(), "an unstyled tooltip reports that it was unstyled");
	const auto potion = TooltipText::Split(plain, BaseStats);
	Check(potion.headline == "Minor Healing Potion", "an item with no base damage is just its name");
	Check(potion.detail == "Restores Health", "the rest is still offered");

	// Without the game's labels the headline is still the name alone.
	const auto unlabelled = TooltipText::Split(TooltipText::Read(Stored()), {});
	Check(unlabelled.headline == "Bloodletter", "no labels, no base-stat line in the headline");
	Check(unlabelled.detail.find("One-Hand Damage: 7 to 15") != std::string::npos, "and nothing is lost -- it moves to the detail");

	// The two bodies the user heard read wrongly on 2026-09-20: unstyled,
	// stored bottom-up, every line drawn with font index 0. The belt slot spoke
	// "Points: 45" for the name and held the name back for the right stick.
	const auto belt = TooltipText::Split(TooltipText::Read(
		Font0 + "Points: 45\n" + Font0 + "Minor Healing Potion"), BaseStats);
	Check(belt.headline == "Minor Healing Potion", "an unstyled body names the item from the line drawn on top");
	Check(belt.detail == "Points: 45", "and the bare number it used to speak is held for the asking");
	const auto sword = TooltipText::Split(TooltipText::Read(
		Font0 + "Sword Class - Fast Attack Speed\n" + Font0 + "Max Sockets: 2\n"
		+ Font0 + "Durability: 24 of 24\n" + Font0 + "One-Hand Damage: 2 to 7\n"
		+ Font0 + "Short Sword"), BaseStats);
	Check(sword.headline == "Short Sword", "equipment is named alone");
	Check(sword.detail == "One-Hand Damage: 2 to 7. Durability: 24 of 24. Max Sockets: 2. Sword Class - Fast Attack Speed",
		"and the rest, base damage first, follows the screen downwards");

	// In a shop: the name and the price on the move, everything else on the
	// right stick. MEASURED 2026-09-21 selling a staff: "Sell value" was not a
	// known label, so it was read as the name.
	const std::vector<std::string> Prices{"Cost:", "Sell value:"};
	const auto selling = TooltipText::Split(TooltipText::Read(
		Font0 + "+2 to Life\n" + Font0 + "Durability: 5 of 6\n" + Font0 + "One-Hand Damage: 1 to 3\n"
		+ Font0 + "Low Quality Eagle Orb\n" + Font0 + "Sell value: 73"), BaseStats, Prices);
	Check(selling.headline == "Low Quality Eagle Orb. Sell value: 73", "an item being sold reads its name and what it fetches");
	Check(selling.detail == "One-Hand Damage: 1 to 3. Durability: 5 of 6. +2 to Life",
		"and its base damage waits with the rest of the tooltip");
	const auto buying = TooltipText::Split(TooltipText::Read("Defense: 8\nCap\nCost: 30"), BaseStats, Prices);
	Check(buying.headline == "Cap. Cost: 30" && buying.detail == "Defense: 8", "buying reads the name and the price the same way");

	// The font index is markup, not text. Item tooltips draw with index 0,
	// which used to survive and be spoken as a stray "0" on every line.
	const auto indexed = TooltipText::Read(Font0 + "Short Sword");
	Check(indexed.lines.size() == 1 && indexed.lines[0] == "Short Sword", "the font index is not spoken");
	Check(!indexed.Marked(), "and index 0 is not the title styling");
	const auto highest = TooltipText::Read(std::string(Title).substr(0, 3) + "VShort Sword");
	Check(highest.lines.size() == 1 && highest.lines[0] == "Short Sword", "nor is the highest index the renderer accepts");
	const auto beyond = TooltipText::Read(std::string(Title).substr(0, 3) + "WShort Sword");
	Check(beyond.lines.size() == 1 && beyond.lines[0] == "WShort Sword", "a character past that range is text, as the renderer reads it");

	Check(TooltipText::Read("").lines.empty(), "an empty tooltip has no lines");
	Check(TooltipText::Split(TooltipText::Read(""), BaseStats).Empty(), "and nothing to say");
	Check(TooltipText::Read("\n\n\n").lines.empty(), "blank lines are not content");
	Passed();
}
