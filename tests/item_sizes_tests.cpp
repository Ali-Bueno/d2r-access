#include "generated/item_sizes.h"
#include "test_support.h"

using namespace D2RAccess;

// Sizes from the game's own weapons.txt, armor.txt and misc.txt, named so a
// failure says which item changed shape rather than which code.
namespace {
struct Item { const char* name; const char code[4]; uint8_t width, height; };

constexpr Item Items[] = {
	// The ones from the report that started this, 2026-09-21: one short sword
	// named on three cells, and arrows and bolts the game had no room for.
	{"Short Sword",          "ssd", 1, 3},
	{"Hand Axe",             "hax", 1, 3},
	{"Arrows",               "aqv", 1, 3},
	{"Bolts",                "cqv", 1, 3},
	{"Throwing Knife",       "tkf", 1, 2},
	{"Buckler",              "buc", 2, 2},
	{"Leather Gloves",       "lgl", 2, 2},
	{"Quilted Armor",        "qui", 2, 3},
	{"Minor Healing Potion", "hp1", 1, 1},
	{"Scroll of Town Portal","tsc", 1, 1},
	{"Chipped Amethyst",     "gcv", 1, 1},
};
}

int main() {
	for (const auto& item : Items) {
		const auto size = Generated::ItemSizeFor(Generated::ItemCode(item.code));
		Check(size.width == item.width && size.height == item.height, item.name);
	}
	// The game pads a three-letter code with a space; a NUL there is read the same.
	const uint32_t nulPadded = Generated::ItemCode("ssd") & 0x00FFFFFFu;
	Check(Generated::ItemSizeFor(nulPadded).height == 3, "a NUL-padded code finds the same item");
	Check(Generated::ItemSizeFor(Generated::ItemCode("zzz")).width == 0, "an unknown code has no size");
	Passed();
}
