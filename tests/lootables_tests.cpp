#include "generated/door_sounds.h"
#include "generated/lootables.h"
#include "test_support.h"

#include <algorithm>

using namespace D2RAccess;

// Object class ids from the game's own objects.txt (the *ID column), named so
// a failure says which object stopped being lootable rather than which number.
namespace {
struct Object { const char* name; uint32_t id; bool lootable; };

constexpr Object Objects[] = {
	// The bug this table was built for: a Blood Moor dead rogue is lootable
	// and carries NO container bit, so the old SubClass rule filed it beside
	// levers and doors.
	{"RogueCorpse1",       54,  true},
	{"RogueStakedCorpse1", 57,  true},
	// Containers the container bit also misses.
	{"Barrel",             7,   true},
	{"Urn2",               9,   true},
	{"Casket3",            79,  true},
	{"Chest4",             148, true},
	// Containers it does catch; they must keep working.
	{"LargeChestR",        5,   true},
	{"GuardOnAStick",      178, true},
	// Furniture that drops, which nothing covered before.
	{"ArmorStand1",        104, true},
	{"WeaponRack1",        106, true},
	// The jungle's stone stashes: only the first carries the container bit (a tester, 2026-11-10).
	{"JunglePillar0",      185, true},
	{"JunglePillar3",      188, true},
	// Objects that hand over a fixed quest item.
	{"HoradricCubeChest",  354, true},
	{"Malus",              108, true},
	{"InifussTree",         30,  true},
	{"LamEsensTome",       193, true},
	// Not loot, and must not be swept in: a shrine, a waypoint, a door and
	// the stash all sit in categories of their own.
	{"DeerShrine",         2,   false},
	{"DoorWoodenLeft",     15,  false},
	{"WaypointOutsideAct1",119, false},
	{"Bank",               267, false},
};
}

int main() {
	for (const Object& object : Objects) {
		Check(Generated::LootableObject(object.id) == object.lootable,
			std::string(object.name) + (object.lootable ? " gives loot" : " gives no loot"));
	}
	for (const uint32_t id : {30u, 108u, 193u, 354u, 355u, 356u, 405u, 406u, 407u})
		Check(Generated::QuestItemObject(id), "fixed quest item object is classified separately");
	// Dead bodies loop the corpse cue: rogues, guards, villagers, barbarians, the hung skeletons.
	for (const uint32_t id : {54u, 57u, 154u, 178u, 271u, 314u, 363u, 477u, 540u})
		Check(Generated::CorpseObject(id) && Generated::LootableObject(id), "a lootable dead body is a corpse");
	for (const uint32_t id : {5u, 7u, 9u, 79u, 148u, 169u, 364u})
		Check(!Generated::CorpseObject(id), "a chest, barrel, urn, casket, log or the unlabelled dungeon guy is not a corpse");
	// Weapon racks loop their own cue; armor stands keep the chest one.
	for (const uint32_t id : {106u, 107u, 548u, 549u})
		Check(Generated::WeaponRackObject(id) && Generated::LootableObject(id), "a weapon rack is lootable and told apart");
	for (const uint32_t id : {104u, 5u, 54u})
		Check(!Generated::WeaponRackObject(id), "an armor stand, a chest or a corpse is not a weapon rack");
	Check(Generated::TomeObject(8) && Generated::TomeObject(357), "the Moldy Tome and Horazon's journal are read in place");
	Check(!Generated::TomeObject(193) && Generated::QuestItemObject(193), "Lam Esen's Tome hands over an item instead");
	// Door opening sounds as the game assigns them, names notwithstanding.
	using Generated::DoorSound;
	Check(Generated::DoorOpenSound(15) == DoorSound::Wood && Generated::DoorOpenSound(294) == DoorSound::Wood, "wooden doors creak");
	Check(Generated::DoorOpenSound(13) == DoorSound::Metal && Generated::DoorOpenSound(292) == DoorSound::Metal,
		"the Act 1 gate and the desert wooden grate both use the metal sound");
	Check(Generated::DoorOpenSound(91) == DoorSound::Stone && Generated::DoorOpenSound(23) == DoorSound::Gate
		&& Generated::DoorOpenSound(229) == DoorSound::Slime, "tomb, courtyard and slime doors keep their own sounds");
	Check(Generated::DoorOpenSound(5) == DoorSound::None && Generated::DoorOpenSound(74) == DoorSound::None,
		"a chest and the trapdoor are not doors");
	for (const uint32_t id : {7u, 15u, 54u, 119u, 267u})
		Check(!Generated::QuestItemObject(id), "ordinary object is not a quest item source");
	Check(!Generated::QuestItemObject(0xFFFFFFFFu), "out-of-range quest object id is rejected");
	Check(!Generated::QuestItemObject(252) && !Generated::LootableObject(252), "the Gidbinn's altar wakes a guardian, it hands over nothing");
	// Quest mechanisms by objects.txt *ID: Cain's Gibbet, the Tainted Sun altar, the orifice,
	// the Gidbinn's altar, the sewer lever, the Hellforge, the five seals, the Compelling Orb,
	// both Altars of the Heavens and Frozen Anya.
	for (const uint32_t id : {26u, 149u, 152u, 252u, 367u, 376u, 392u, 393u, 394u, 395u, 396u, 404u, 546u, 558u, 573u})
		Check(Generated::QuestMechanismObject(id) && !Generated::LootableObject(id), "a quest mechanism is told apart from loot");
	// The sewer stairs, the Ancients' statues, the stash, a waypoint and a chest are not.
	for (const uint32_t id : {366u, 474u, 475u, 476u, 267u, 119u, 5u, 0xFFFFFFFFu})
		Check(!Generated::QuestMechanismObject(id), "an ordinary object is not a quest mechanism");
	for (uint32_t ordinal = 1; ordinal <= Generated::SealObjects.size(); ++ordinal)
		Check(Generated::SealOrdinal(391 + ordinal) == ordinal && Generated::QuestMechanismObject(391 + ordinal),
			"DiabloSeal1 to DiabloSeal5 are numbered in class order");
	Check(!Generated::SealOrdinal(404) && !Generated::SealOrdinal(0xFFFFFFFFu), "only a seal has a seal number");
	// Object modes as objects.txt orders them: 0 neutral, 1 operating, 2 opened, 3 the first special.
	Check(Generated::QuestMechanismStep(404, 0) && !Generated::QuestMechanismStep(404, 1) && !Generated::QuestMechanismStep(404, 2),
		"the Compelling Orb is a quest step only while neutral");
	Check(Generated::QuestMechanismStep(376, 0) && Generated::QuestMechanismStep(376, 2) && !Generated::QuestMechanismStep(376, 3),
		"the Hellforge stays one while it holds the Soulstone, until it is broken");
	Check(!Generated::QuestMechanismStep(152, 2), "the orifice holding the staff is done");
	Check(!Generated::QuestMechanismStep(5, 0) && !Generated::QuestMechanismStep(404, 8) && !Generated::QuestMechanismStep(0xFFFFFFFFu, 0),
		"an ordinary object or an impossible mode is no quest step");
	Check(Generated::QuestMechanismModes.size() == Generated::QuestMechanismObjects.size(), "every mechanism has its modes");
	const auto& mechanismIds = Generated::QuestMechanismObjects;
	Check(std::is_sorted(mechanismIds.begin(), mechanismIds.end())
		&& std::adjacent_find(mechanismIds.begin(), mechanismIds.end()) == mechanismIds.end(), "quest mechanism table is sorted and unique");

	// The lookup is a binary search, so the table has to stay sorted, and a
	// duplicate id would mean the generator emitted a row twice.
	const auto& ids = Generated::LootableObjects;
	Check(std::is_sorted(ids.begin(), ids.end()), "the table is sorted");
	Check(std::adjacent_find(ids.begin(), ids.end()) == ids.end(), "no id appears twice");
	Check(!ids.empty(), "the table is not empty");
	const auto& questIds = Generated::QuestItemObjects;
	Check(std::is_sorted(questIds.begin(), questIds.end()), "quest object table is sorted");
	Check(std::adjacent_find(questIds.begin(), questIds.end()) == questIds.end(), "quest object table has no duplicates");
	// A class id past the table cannot read off the end.
	Check(!Generated::LootableObject(0xFFFFFFFFu), "an out-of-range class id is not lootable");

	Passed();
	return 0;
}
