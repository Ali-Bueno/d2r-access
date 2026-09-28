#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// What the game tells a sighted player about a monster beyond its base name,
// turned into words to speak. Pure, so it is unit-tested without the game.
namespace D2RAccess::MonsterPlate {
// One of a phrase's grammatical forms: "[ms]fanatischer[fs]fanatische" holds
// one per gender tag. The form for `tag` ("[fs]") when the phrase has it,
// otherwise its first; a phrase with no tags comes back whole.
std::string Form(std::string_view text, std::string_view tag = {});
// A name the game composes from a format and its parts, as its name builder
// (0x1400985A0) does: a champion's "%0 %1" (its kind, then the base name), a
// random unique's "%0 %1" or "%0 %1 %2". A format opening "a<i>n<j>:" makes
// part i an adjective agreeing with part j, the noun, by the noun's tag.
std::string Compose(std::string_view format, std::span<const std::string> parts);

// MonsterData (*(Unit+0x10)) type flags, a 16-bit word at +0x1A (tested by 0x38E870).
inline constexpr uint16_t FlagChampion = 0x04, FlagMinion = 0x10;
// The game's special-monster test (0x38E0E0): champions, uniques, super uniques.
inline constexpr uint16_t SpecialMask = 0x0F;
// Enchantment slots at MonsterData+0x20 (0x38E310), 0 for an empty one.
inline constexpr size_t Slots = 9;
// MonStats row flag byte +0x3D: monstats.txt lUndead, hUndead and demon.
inline constexpr uint8_t RowUndead = 0x08, RowHighUndead = 0x10, RowDemon = 0x20;
// The game's string keys for a monster's nature and for a minion.
inline constexpr std::string_view DemonKey = "DemonID", UndeadKey = "UndeadDescriptX", MinionKey = "minion";
// The game's own tables, read from the image: the string id naming each
// enchantment, 0 for one the plate never shows (0x141CDD580), and a champion's
// kind word by the enchantment that makes it one (0x141CDD4F0).
inline constexpr size_t EnchantmentCount = 45, ChampionKindCount = 5;
struct ChampionKind { uint32_t enchantment = 0, text = 0; };
struct Tables {
	std::array<uint16_t, EnchantmentCount> enchantments{};
	std::array<ChampionKind, ChampionKindCount> champions{};
};

enum class Rank { Normal, Champion, Unique, Minion };
// What the game shows about a monster beyond its name: the name's colour
// (champion blue, unique gold) and the line under its health plate, built by
// 0x1400CEA50: a special monster's nature, then each enchantment it names in
// slot order (0x1401DF6B0); a minion's nature, then "Minion" (0x1401DF830).
struct Plate {
	Rank rank = Rank::Normal;
	uint32_t championKind = 0;           // string id of a champion's kind (Champion, Fanatic, ...), 0 when none
	std::string_view natureKey;          // DemonKey or UndeadKey, empty when neither or for an ordinary monster
	std::vector<uint16_t> enchantments;  // string ids
	std::vector<std::string_view> immunities; // keys, for any monster (Immune)
};
Plate Read(uint16_t flags, std::span<const uint8_t, Slots> slots, uint8_t rowFlags, const Tables& tables);
// The words said after the monster's name, joined by `separator`: a unique's
// `unique` (its gold says nothing aloud), a champion's kind when `named` is false
// (a composed name already carries it), then the plate's line, immunities last.
struct Words {
	std::function<std::string(uint32_t)> byId;
	std::function<std::string(std::string_view)> byKey;
	std::string_view unique, separator;
};
std::string Spoken(const Plate& plate, bool named, const Words& words);

// The immunities the game names under any monster's health plate (0x1400CEA50),
// in its order: the monster's resistance, less the player's pierce of it, from
// 100 up. Stat ids are ItemStatCost.txt rows; the keys, the game's strings.
struct Immunity { int resist, pierce; std::string_view key; };
inline constexpr std::array<Immunity, 6> Immunities{{
	{36, 192, "strScaryMonDesc5"}, // physical
	{37, 193, "strScaryMonDesc6"}, // magic
	{39, 189, "strScaryMonDesc1"}, // fire
	{41, 190, "strScaryMonDesc3"}, // lightning
	{43, 187, "strScaryMonDesc2"}, // cold
	{45, 191, "strScaryMonDesc4"}, // poison
}};
inline constexpr int ImmuneFrom = 100;
// The uber bosses' MonStats rows, whose plate the game gives no immunities.
inline constexpr uint32_t UberFirst = 704, UberLast = 709;
std::vector<std::string_view> Immune(std::span<const int, Immunities.size()> resist, std::span<const int, Immunities.size()> pierce);
}
