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
// The name is found by the font-size command the renderer marks the heading
// with, so the split does not depend on which end of the body the engine
// happens to store the top line at. Where there is no marker the first line
// stands in, and `Body::heading` says which of the two happened, so a log can
// show it rather than leaving it assumed.
//
// Pure text handling: no native calls, no SDK, unit-tested without the game.
namespace D2RAccess::TooltipText {

struct Body {
	std::vector<std::string> lines;  // as stored, cleaned, empties dropped
	size_t heading = static_cast<size_t>(-1); // the line the renderer styled as the title
	bool Marked() const { return heading != static_cast<size_t>(-1); }
};

Body Read(std::string_view body);

struct Description {
	std::string headline; // the name, and the base damage or defense
	std::string detail;   // requirements, properties, flavour text, price
	bool Empty() const { return headline.empty() && detail.empty(); }
};

// `baseStats` are the game's own localized labels that begin a base damage or
// defense line ("One-Hand Damage: ", "Defense: "). They are passed in rather
// than resolved here so this stays testable, and an empty set simply leaves
// the headline as the item's name.
Description Split(const Body& body, std::span<const std::string> baseStats);

// The string-table keys for those labels. Unknown keys resolve to nothing,
// which costs only a shorter headline.
inline constexpr const char* BaseStatKeys[] = {
	"ItemStats1d", // Defense:
	"ItemStats1h", // One-Hand Damage:
	"ItemStats1i", // Two-Hand Damage:
	"ItemStats1j", // Throw Damage:
	"ItemStats1k", // Smite Damage:
	"ItemStats1l", // Damage:
};

}
