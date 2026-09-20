#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The stock tooltip body, separated into what a player wants on every move and
// what they want only when they ask.
//
// An item's name and its base damage or defense are the headline: those are
// read as the highlight moves. Its requirements, its magic properties, its
// flavour text and its price are the detail, offered on demand instead.
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
	size_t heading = static_cast<size_t>(-1); // the line the renderer styled as the title
	bool Marked() const { return heading != static_cast<size_t>(-1); }
};

Body Read(std::string_view body);

struct Description {
	std::string headline; // the name, and the base damage or defense
	std::string detail;   // requirements, properties, flavour text, price
	bool Empty() const { return headline.empty() && detail.empty(); }
};

// `headlineLabels` are the game's own localized labels for the lines that
// belong beside the item's name: its base damage or defense, and its price in
// a shop. They are passed in rather than resolved here so this stays testable,
// and an empty set simply leaves the headline as the item's name.
//
// They also decide which line the NAME is: the name is the first line that is
// not one of them. A vendor tooltip puts its price line where an inventory one
// has the name, so taking the first line outright read "Cost: 30" as the item.
Description Split(const Body& body, std::span<const std::string> headlineLabels);

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

// What a vendor charges. Its own key because it is the one headline line that
// is not a property of the item, and because it is the line that used to be
// read as the item's name.
inline constexpr const char* PriceKey = "cost"; // Cost: %d

}
