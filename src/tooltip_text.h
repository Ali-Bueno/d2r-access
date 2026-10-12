#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The stock tooltip body, separated into what a player wants on every move and
// what they want only when they ask.
//
// An item's name is the headline, read as the highlight moves -- with its
// price beside it in a shop. Everything else, its base damage or defense
// included, is the detail, offered on demand instead (the user, 2026-09-21:
// "even base stat should go on description reader in all cases").
//
// The engine stores a body bottom-up, so `Read` reverses it and `Body::lines`
// is the tooltip in the order the screen draws it -- the name first. The name
// is taken from the font command the renderer marks a heading with where there
// is one, and from that first line where there is not; `Body::heading` says
// which of the two happened, so a log can show it rather than leaving it
// assumed.
//
// Pure text handling: no native calls, no SDK, unit-tested without the game.
namespace D2RAccess::TooltipText {

struct Body {
	std::vector<std::string> lines;  // as drawn, top first, cleaned, empties dropped
	std::vector<bool> red;           // per line: drawn in the game's red (unmet requirements)
	size_t heading = static_cast<size_t>(-1); // the line the renderer styled as the title
	bool Marked() const { return heading != static_cast<size_t>(-1); }
};

Body Read(std::string_view body);

struct Description {
	std::string headline; // the name, and in a shop the price
	// The name and the price apart, so something can be read between them.
	std::string name, price;
	std::string detail;   // base damage or defense, requirements, properties, flavour text
	std::string unmet;    // the red requirement lines: what this character cannot meet
	bool unidentified = false; // the tooltip's "Unidentified" line, said after the name, not in the detail
	bool Empty() const { return headline.empty() && detail.empty(); }
};

// `statLabels` are the game's own localized labels for an item's base damage
// or defense, and `priceLabels` those for what a vendor charges or pays. A
// price joins the name in the headline; a base stat is detail like the rest.
// They are passed in rather than resolved here so this stays testable, and an
// empty set simply leaves the headline as the item's name.
//
// Both also decide which line the NAME is: the name is the first line that is
// none of them. A vendor tooltip puts its price line where an inventory one has
// the name, so taking the first line outright read "Cost: 30" as the item.
// `unidentifiedLabel` is the game's "Unidentified" line: it follows the name
// and leaves the detail. `requirementLabels` begin the lines the game draws red
// when unmet; those red lines also go to `unmet`, and stay in the detail.
// `quantityLabels` begin a stack's count line, which moves into the headline
// between the name and the price.
Description Split(const Body& body, std::span<const std::string> statLabels,
	std::span<const std::string> priceLabels = {}, std::string_view unidentifiedLabel = {},
	std::span<const std::string> requirementLabels = {}, std::span<const std::string> quantityLabels = {});

// What a held compare button adds, as the screen draws it: each extra record's
// lines ("Currently Equipped", then the equipped item), joined for speech.
// Button-image records clean to nothing and are left out; empty when there is
// no comparison.
std::string Comparison(std::span<const std::string> extras);

// Diagnostic: the inline markup of each raw body line that carries any (colour
// escapes, private-use renderer commands, control bytes), for the log.
std::string Markup(std::string_view body);

// A table entry is a printf format ("Defense: %d", "One-Hand Damage: %d to
// %d"), so the part of it that actually begins a rendered line is everything
// up to its first placeholder. Returns that prefix, trimmed.
std::string LabelPrefix(std::string_view entry);

// The string-table keys for those labels, VERIFIED against the game's own
// local/lng/strings/*.json (D2R 3.3, extracted from CASC). The previous list
// was wrong on every entry -- ItemStats1d is "Durability", 1i is "Quantity"
// and 1j is the bare word "of" -- so it matched nothing it meant to and could
// have matched a great deal it did not. Unknown keys resolve to nothing, which
// costs only a shorter headline.
inline constexpr const char* BaseStatKeys[] = {
	"ItemStats1g",      // Damage:
	"ItemStats1h",      // Defense: %d
	"ItemStats1hRange", // Defense: (%d-%d)
	"ItemStats1l",      // One-Hand Damage: %d to %d
	"ItemStats1m",      // Two-Hand Damage: %d to %d
	"ItemStats1n",      // Throw Damage: %d
	"ItemStats1o",      // Smite Damage: %d to %d
	"strItemStatThrowDamageRange", // Throw Damage: %d to %d
};

// What a vendor charges. Its own key because it is the headline line that is
// not the item, and because it is the line that used to be read as the item's
// name.
inline constexpr const char* PriceKey = "cost"; // Cost: %d

// What a vendor would pay, shown on the player's own items while selling --
// VERIFIED against local/lng/strings/ui.json. Unknown to the splitter before,
// so it was read as the item's name (MEASURED 2026-09-21).
inline constexpr const char* SellKey = "Sell"; // Sell value: %d

// A stack's count ("Quantity: 20 of 20") joins the name in the headline, as
// does a tome's: stackables use ItemStats1i, tomes scrollbooktext.
inline constexpr const char* QuantityKeys[] = {"ItemStats1i", "scrollbooktext"};

// Verified in local/lng/strings/item-modifiers.json for all game locales.
inline constexpr const char* UnidentifiedKey = "ItemStats1b";

// Requirements a tooltip draws red when this character does not meet them,
// from local/lng/strings/item-modifiers.json: strength, dexterity, level, class.
inline constexpr const char* RequirementKeys[] = {
	"ItemStats1e", "ItemStats1f", "ItemStats1p", // Required Strength / Dexterity / Level: %d
	"AmaOnly", "SorOnly", "NecOnly", "PalOnly", "BarOnly", "DruOnly", "AssOnly", "WarOnly", // (<Class> Only)
};

}
