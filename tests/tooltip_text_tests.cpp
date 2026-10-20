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
	const auto set = TooltipText::Read(
		std::string(Title) + "Other Set Piece\n" + Title + "Set Collection\n"
		+ Font0 + "One-Hand Damage: 3 to 8\n" + Title + "Sabre\n"
		+ std::string(Title).substr(0, 3) + "3Angelic Sickle");
	const auto setSplit = TooltipText::Split(set, BaseStats, {}, {}, {}, {}, true);
	Check(setSplit.name == "Angelic Sickle", "set-name screen order outranks palette-2 type and set-list lines");
	Check(setSplit.detail == "Sabre. One-Hand Damage: 3 to 8. Set Collection. Other Set Piece",
		"set details retain every other drawn line in its original order");
	const std::vector<std::string> setPrices{"Cost:"};
	const auto soldSet = TooltipText::Split(TooltipText::Read(std::string(Title) + "Sabre\n"
		+ std::string(Title).substr(0, 3) + "3Angelic Sickle\nCost: 100"), BaseStats, setPrices, {}, {}, {}, true);
	Check(soldSet.headline == "Angelic Sickle. Cost: 100", "vendor price does not replace a set item's displayed name");

	const auto ordered = TooltipText::Split(TooltipText::Read(Stored()), BaseStats);
	Check(ordered.detail == "Short Sword. One-Hand Damage: 7 to 15. Required Strength: 25. +15 to Attack Rating. Sell price: 84 gold",
		"the detail is the rest of the tooltip in order");

	// Armour reads the same way through its own label.
	const auto armour = TooltipText::Split(TooltipText::Read("Required Strength: 20\nDefense: 43\nQuilted Armor"), BaseStats);
	Check(armour.headline == "Quilted Armor", "armour is named alone too");
	Check(armour.detail == "Defense: 43. Required Strength: 20", "and its defense leads the detail, where the screen draws it");

	// Red (the style command with palette index '1') is how the game marks what
	// this character cannot meet. Stored bottom-up, so the screen order is reversed.
	const std::string red = std::string(Title).substr(0, 3) + "1";
	const std::vector<std::string> requirements{"Required Strength:", "Required Level:", "(Paladin Only)"};
	const auto heavy = TooltipText::Split(TooltipText::Read(red + "Required Strength: 40\nRequired Level: 3\n" + red
		+ "(Paladin Only)\n" + red + "Durability: 0 of 20\nDefense: 43\nGothic Plate"), BaseStats, {}, {}, requirements);
	Check(heavy.unmet == "(Paladin Only). Required Strength: 40", "the red requirement lines are what the character cannot meet");
	Check(heavy.detail.find("Required Strength: 40") != std::string::npos, "they stay in the detail too");
	Check(heavy.unmet.find("Durability") == std::string::npos, "red that is not a requirement is not a reason");
	Check(heavy.headline == "Gothic Plate" && armour.unmet.empty(), "requirements in the ordinary colour are met");
	Check(TooltipText::Markup(red + "Required Strength: 40\nDefense: 43") == "L0[U+E07E:1] Required Strength: 40 | ",
		"the markup diagnostic names each styled line's commands, index included");
	Check(TooltipText::Markup("Defense: 43\nQuilted Armor").empty(), "and says nothing for plain lines");

	// A throwing potion: a styled damage line under the styled name, or the only
	// styled line, never takes the name's place (a tester heard "Throw Damage" first).
	const std::vector<std::string> throwStats{"Throw Damage:"};
	const auto gas = TooltipText::Read("Required Level: 6\n" + std::string(Title) + "Throw Damage: 7 to 9\n"
		+ std::string(Title) + "Strangling Gas Potion");
	Check(gas.headings.size() == 2 && gas.heading == 0, "every styled line is kept, the topmost first");
	Check(TooltipText::Split(gas, throwStats).headline == "Strangling Gas Potion", "the styled name wins over a styled stat below it");
	const auto lone = TooltipText::Read("Required Level: 6\n" + std::string(Title) + "Throw Damage: 7 to 9\nStrangling Gas Potion");
	Check(TooltipText::Split(lone, throwStats).headline == "Strangling Gas Potion", "a styled stat alone does not name the item");

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
	Check(buying.name == "Cap" && buying.price == "Cost: 30", "the name and the price are also kept apart, so an item's size can go between them");
	// "Unidentified" is the name's status: said right after it, and not again in the detail.
	const auto unknown = TooltipText::Split(TooltipText::Read("Unidentified\nDefense: 8\nCap\nCost: 30"), BaseStats, Prices, "Unidentified");
	Check(unknown.unidentified && unknown.name == "Cap. Unidentified" && unknown.headline == "Cap. Unidentified. Cost: 30",
		"an unidentified item says so after its name, ahead of the price");
	Check(unknown.detail == "Defense: 8", "and the detail no longer carries it");
	Check(!buying.unidentified, "an identified item has no such status");
	// Holding the compare button adds the equipped item as another record.
	const std::vector<std::string> extras{"Defense: 2\nLeather Gloves\nCurrently Equipped", "\xEE\x80\x93"};
	Check(TooltipText::Comparison(extras) == "Currently Equipped. Leather Gloves. Defense: 2", "a comparison reads as the screen draws it, and a button image adds nothing");
	Check(TooltipText::Comparison({}).empty(), "no extra record, no comparison");

	// The font index is markup, not text. Item tooltips draw with index 0,
	// which used to survive and be spoken as a stray "0" on every line.
	const auto indexed = TooltipText::Read(Font0 + "Short Sword");
	Check(indexed.lines.size() == 1 && indexed.lines[0] == "Short Sword", "the font index is not spoken");
	Check(!indexed.Marked(), "and index 0 is not the title styling");
	const auto highest = TooltipText::Read(std::string(Title).substr(0, 3) + "VShort Sword");
	Check(highest.lines.size() == 1 && highest.lines[0] == "Short Sword", "nor is the highest index the renderer accepts");
	const auto beyond = TooltipText::Read(std::string(Title).substr(0, 3) + "WShort Sword");
	Check(beyond.lines.size() == 1 && beyond.lines[0] == "WShort Sword", "a character past that range is text, as the renderer reads it");

	// A tome's count is read with its name, ahead of any price.
	const std::vector<std::string> quantity{"Quantity:"};
	{
		auto described = TooltipText::Split(TooltipText::Read("Quantity: 20 of 20\nTome of Town Portal"), BaseStats, {}, {}, {}, quantity);
		Check(described.stack == "Quantity: 20 of 20", "the count line is kept apart for the reader");
		described.name += ". magic"; // the reader adds words to the name after Split
		Check(TooltipText::Headline(described) == "Tome of Town Portal. magic. Quantity: 20 of 20", "a rebuilt headline keeps the count");
	}
	const auto tome = TooltipText::Split(TooltipText::Read("Quantity: 20 of 20\nTome of Identify"), BaseStats, {}, {}, {}, quantity);
	Check(tome.name == "Tome of Identify" && tome.headline == "Tome of Identify. Quantity: 20 of 20" && tome.detail.empty(),
		"a tome's quantity joins the name in the headline");
	const std::vector<std::string> costs{"Cost:"};
	const auto stack = TooltipText::Split(TooltipText::Read("Quantity: 5 of 60\nCost: 30\nThrowing Knife"), BaseStats, costs, {}, {}, quantity);
	Check(stack.name == "Throwing Knife" && stack.headline == "Throwing Knife. Quantity: 5 of 60. Cost: 30", "the quantity precedes the price");
	Check(TooltipText::Split(TooltipText::Read("Quantity: 5 of 60\nThrowing Knife"), BaseStats).detail == "Quantity: 5 of 60",
		"without quantity labels the line stays detail");

	Check(TooltipText::Read("").lines.empty(), "an empty tooltip has no lines");
	Check(TooltipText::Split(TooltipText::Read(""), BaseStats).Empty(), "and nothing to say");
	Check(TooltipText::Read("\n\n\n").lines.empty(), "blank lines are not content");
	Passed();
}
