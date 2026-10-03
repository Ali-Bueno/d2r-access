#include "monster_plate.h"
#include "test_support.h"

#include <array>
#include <map>
#include <string>
#include <vector>

using namespace D2RAccess;

namespace {
// The game's own strings (local/lng/strings: strChampionFormatXp, Monster2Format,
// champfanaticX, Champion, Fallen).
void Forms() {
	using MonsterPlate::Form;
	Check(Form("Gefallener") == "Gefallener", "a phrase with no tags comes back whole");
	Check(Form("[ms]Gefallener") == "Gefallener", "a lone tag is not spoken");
	Check(Form("[ms]fanatischer[fs]fanatische[ns]fanatisches[pl]fanatische") == "fanatischer", "the first form by default");
	Check(Form("[ms]fanatischer[fs]fanatische[ns]fanatisches[pl]fanatische", "[fs]") == "fanatische", "the form for a tag");
	Check(Form("[ms]fanatischer[fs]fanatische", "[mp]") == "fanatischer", "a tag the phrase lacks takes the first form");
	Check(Form("").empty(), "an empty phrase stays empty");
}
void Names() {
	using MonsterPlate::Compose;
	std::vector<std::string> parts{"Champion", "Fallen"};
	Check(Compose("%0 %1", parts) == "Champion Fallen", "an English champion: its kind, then the base name");
	parts = {"[ms]fanatischer[fs]fanatische[ns]fanatisches[pl]fanatische", "Gefallener"};
	Check(Compose("a0n1:%0 %1", parts) == "fanatischer Gefallener", "the grammar header is not spoken; an untagged noun is masculine");
	parts = {"[ms]fanatischer[fs]fanatische[ns]fanatisches[pl]fanatische", "[fs]Hexe"};
	Check(Compose("a0n1:%0 %1", parts) == "fanatische Hexe", "the adjective agrees with the noun's tag");
	parts = {"[fs]campeona[ms]campeón[fp]campeonas[mp]campeones", "Caído"};
	Check(Compose("a0n1:%1 %0", parts) == "Caído campeón", "a format may put the noun first");
	parts = {"элита", "Падший"};
	Check(Compose("%1 (%0)", parts) == "Падший (элита)", "a format with no header fills its parts as they are");
	parts = {"Gloom", "Spawn", "the Hungry"};
	Check(Compose("%0 %1 %2", parts) == "Gloom Spawn the Hungry", "a random unique's three parts");
	parts = {"Gloom", "Spawn"};
	Check(Compose("%0 %1 %2", parts) == "Gloom Spawn", "a missing part leaves no trailing space");
	Check(Compose("100%", parts) == "100%", "a lone percent sign is kept");
}

// The game's tables as read from the 3.3.93787 image (0x141CDD580, 0x141CDD4F0)
// and its enUS strings (local/lng/strings/monsters.json, item-nameaffixes.json).
MonsterPlate::Tables GameTables() {
	MonsterPlate::Tables tables;
	const std::map<size_t, uint16_t> names{{5, 3205}, {6, 3206}, {7, 3207}, {8, 3208}, {9, 3209}, {17, 3211}, {18, 3210},
		{24, 3217}, {25, 3212}, {26, 3214}, {27, 3213}, {28, 3215}, {29, 3216}, {30, 3218}};
	for (const auto& [slot, id] : names) tables.enchantments[slot] = id;
	tables.champions = {{{16, 3220}, {36, 11084}, {37, 11085}, {38, 11086}, {39, 11087}}};
	return tables;
}
const std::map<uint32_t, std::string> English{{3205, "Extra Strong"}, {3206, "Extra Fast"}, {3208, "Magic Resistant"},
	{3209, "Fire Enchanted"}, {3211, "Lightning Enchanted"}, {3218, "Aura Enchanted"}, {3220, "Champion"},
	{11085, "[ms]fanatischer[fs]fanatische"}};
MonsterPlate::Words Words() {
	return {[](uint32_t id) { const auto found = English.find(id); return found == English.end() ? std::string{} : found->second; },
		[](std::string_view key) {
			return std::string(key == MonsterPlate::DemonKey ? "Demon" : key == MonsterPlate::UndeadKey ? "Undead"
				: key == MonsterPlate::MinionKey ? "Minion" : "");
		}, "unique", ", "};
}
std::string Say(uint16_t flags, std::array<uint8_t, MonsterPlate::Slots> slots, uint8_t row, bool named = true,
	std::string_view name = "Fallen") {
	return MonsterPlate::Spoken(MonsterPlate::Read(flags, slots, row, GameTables()), name, named, Words());
}
void Plates() {
	using MonsterPlate::Rank;
	const auto tables = GameTables();
	std::array<uint8_t, MonsterPlate::Slots> slots{};
	Check(MonsterPlate::Read(0, slots, MonsterPlate::RowDemon, tables).rank == Rank::Normal && Say(0, slots, MonsterPlate::RowDemon).empty(),
		"an ordinary monster has no plate, not even its nature");
	slots = {8, 9};
	Check(Say(0x0B, slots, MonsterPlate::RowDemon) == "unique, Demon, Magic Resistant, Fire Enchanted",
		"a super unique: the unique word for its gold, its nature, its enchantments in slot order");
	slots = {17};
	Check(Say(0x09, slots, 0) == "unique, Lightning Enchanted", "the lightning enchantment that strikes back is named");
	slots = {16, 5};
	const auto champion = MonsterPlate::Read(0x0D, slots, 0, tables);
	Check(champion.rank == Rank::Champion && champion.championKind == 3220 && champion.enchantments.size() == 1,
		"a champion's kind comes from its enchantment, which the plate itself never lists");
	Check(Say(0x0D, slots, 0) == "Extra Strong", "a composed champion name already says Champion");
	Check(Say(0x0D, slots, 0, false) == "Champion, Extra Strong", "a champion named by its base name says its kind");
	slots = {37};
	Check(Say(0x0D, slots, 0, false) == "fanatischer", "a fanatic champion's kind, in its first form");
	slots = {9, 30};
	Check(Say(0x10, slots, MonsterPlate::RowUndead) == "Undead, Minion", "a minion: its nature, then Minion, never enchantments");
	Check(Say(0x10, slots, MonsterPlate::RowHighUndead) == "Undead, Minion", "high undead are undead too");
	Check(Say(0x10, slots, MonsterPlate::RowDemon | MonsterPlate::RowUndead) == "Demon, Minion", "demon wins over undead, as in the game");
	Check(Say(0x10, slots, MonsterPlate::RowDemon, true, "Minion of Fallen") == "Demon", "a word the name already says is not said twice");
	slots = {16, 5};
	Check(Say(0x0D, slots, 0, false, "Champion Fallen") == "Extra Strong", "nor a champion's kind its name carries");
	slots = {5, 0, 30, 200, 16};
	Check(Say(0x09, slots, 0) == "unique, Extra Strong, Aura Enchanted",
		"an empty slot is skipped, not an end; an id past the table and a hidden one are never said");
	slots = {7};
	Check(Say(0x09, slots, 0) == "unique", "a word the strings lack is left out, with no stray separator");
}
void Immunities() {
	using MonsterPlate::Immune;
	//                         physical magic fire lightning cold poison
	std::array<int, 6> resist{0, 0, 100, 75, 0, 110}, pierce{};
	auto keys = Immune(resist, pierce);
	Check(keys == std::vector<std::string_view>{"strScaryMonDesc1", "strScaryMonDesc4"}, "fire and poison at 100 and up, in the game's order");
	pierce = {0, 0, 1, 0, 0, 10};
	Check(Immune(resist, pierce) == std::vector<std::string_view>{"strScaryMonDesc4"}, "the player's pierce breaks an immunity below 100");
	resist = {100, 100, 0, 100, 100, 0}; pierce = {};
	Check(Immune(resist, pierce) == std::vector<std::string_view>{"strScaryMonDesc5", "strScaryMonDesc6", "strScaryMonDesc3", "strScaryMonDesc2"},
		"physical, magic, lightning, cold: the game's order, not the strings' numbering");
	resist = {99, 0, 0, 0, 0, 0}; pierce = {-5, 0, 0, 0, 0, 0};
	Check(Immune(resist, pierce).empty(), "a resistance under 100 is never an immunity, whatever the pierce");
	MonsterPlate::Plate plate;
	plate.immunities = {"strScaryMonDesc1"};
	const auto words = MonsterPlate::Words{[](uint32_t) { return std::string{}; },
		[](std::string_view key) { return std::string(key == "strScaryMonDesc1" ? "Immune to Fire" : ""); }, "unique", ", "};
	Check(MonsterPlate::Spoken(plate, "Zombie", true, words) == "Immune to Fire", "an ordinary monster's immunity is said on its own");
	plate.rank = MonsterPlate::Rank::Unique;
	Check(MonsterPlate::Spoken(plate, "Zombie", true, words) == "unique, Immune to Fire", "immunities come last");
}
}

int main() {
	Forms();
	Names();
	Plates();
	Immunities();
	Passed();
}
