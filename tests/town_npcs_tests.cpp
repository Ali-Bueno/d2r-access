#include "town_npcs.h"
#include "test_support.h"

#include <algorithm>

using namespace D2RAccess;
using TownNpcs::Spawn;

namespace {
// MonStats rows as the Rogue Encampment's presets place them; 300 is a guard row, 400 no NPC.
constexpr uint32_t Akara = 148, Gheed = 147, Charsi = 154, Guard = 300, Critter = 400;

std::string Name(uint32_t monster) {
	switch (monster) {
	case Akara: return "Akara";
	case Gheed: return "Gheed";
	case Charsi: return "Charsi";
	case Guard: return "Rogue Guard";
	default: return {};
	}
}

EntityRadar::Entity LiveUnit(uint32_t id, const char* name, Radar::Position position) {
	return {id, EntityRadar::Kind::Npc, position, name, 0, false, EntityRadar::Category::Npc};
}

const EntityRadar::Entity* Find(const std::vector<EntityRadar::Entity>& entities, const std::string& name) {
	const auto found = std::find_if(entities.begin(), entities.end(), [&](const auto& entity) { return entity.name == name; });
	return found == entities.end() ? nullptr : &*found;
}

size_t Count(const std::vector<EntityRadar::Entity>& entities, const std::string& name) {
	return static_cast<size_t>(std::count_if(entities.begin(), entities.end(), [&](const auto& entity) { return entity.name == name; }));
}

void FarNpcsGetStandIns() {
	std::vector<EntityRadar::Entity> entities{LiveUnit(9001, "Charsi", {120, 40})};
	TownNpcs::Merge({{Charsi, {118, 42}, true}, {Akara, {300, 260}}, {Gheed, {210, 300}}}, {{0, Charsi}}, Name, entities);
	Check(entities.size() == 3, "the live NPC stays and each unloaded one gets a stand-in");
	const auto* akara = Find(entities, "Akara");
	Check(akara && akara->position.x == 300 && akara->position.y == 260, "a stand-in stands at its spawn point");
	Check(akara->kind == EntityRadar::Kind::Npc && akara->category == EntityRadar::Category::Npc
		&& akara->placeId == TownNpcs::Identity(Akara) && akara->unit == EntityRadar::Entity::NoUnit,
		"a stand-in is an NPC with its row's identity and no unit to command");
	Check(Find(entities, "Gheed") && Find(entities, "Gheed")->placeId == TownNpcs::Identity(Gheed), "every far NPC is listed");
	Check(Find(entities, "Charsi")->id == 9001 && Find(entities, "Charsi")->placeId == TownNpcs::Identity(Charsi),
		"the live unit keeps its id and gains its row's identity");
}

void LiveUnitSupersedesItsStandIn() {
	std::vector<EntityRadar::Entity> before;
	TownNpcs::Merge({{Akara, {300, 260}}}, {}, Name, before);
	std::vector<EntityRadar::Entity> after{LiveUnit(77, "Akara", {304, 255})};
	TownNpcs::Merge({{Akara, {300, 260}}}, {{0, Akara}}, Name, after);
	Check(Count(after, "Akara") == 1 && after.front().id == 77 && after.front().position.x == 304,
		"once loaded, only the live unit is listed, where it stands");
	Check(before.size() == 1 && EntityRadar::SameTarget(before.front(), after.front()),
		"the stand-in and the live unit are one target, so a beacon on one follows the other");
	std::vector<EntityRadar::Entity> wandered{LiveUnit(77, "Akara", {420, 90})};
	TownNpcs::Merge({{Akara, {300, 260}, true}}, {{0, Akara}}, Name, wandered);
	Check(wandered.size() == 1 && wandered.front().placeId == TownNpcs::Identity(Akara),
		"an NPC that wandered from its spawn is still matched by its row");
}

void SpawnAroundThePlayerWithoutItsUnit() {
	std::vector<EntityRadar::Entity> entities;
	TownNpcs::Merge({{Gheed, {210, 300}, true}}, {}, Name, entities);
	Check(entities.empty(), "a spawn around the player with no unit there is not listed: the NPC is not there");
}

void OnlyNpcsPlacedOnce() {
	std::vector<EntityRadar::Entity> entities{LiveUnit(5, "Rogue Guard", {10, 10})};
	TownNpcs::Merge({{Guard, {10, 10}}, {Guard, {80, 10}}, {Critter, {40, 40}}}, {{0, Guard}}, Name, entities);
	Check(entities.size() == 1 && entities.front().placeId == 0, "guards placed twice and rows that are no NPC add nothing");
}

void NoLiveIndexOutOfRange() {
	std::vector<EntityRadar::Entity> entities;
	TownNpcs::Merge({{Akara, {300, 260}}}, {{4, Akara}}, Name, entities);
	Check(entities.size() == 1 && entities.front().id == TownNpcs::Identity(Akara), "a live entry past the read entities is ignored");
}
}

int main() {
	Check(TownNpcs::CanRetire(297) && TownNpcs::CanRetire(514) && !TownNpcs::CanRetire(526)
		&& !TownNpcs::CanRetire(Gheed), "only verified quest-retired town classes are eligible; hostile Nihlathak and wanderers are separate");
	FarNpcsGetStandIns();
	LiveUnitSupersedesItsStandIn();
	SpawnAroundThePlayerWithoutItsUnit();
	OnlyNpcsPlacedOnce();
	NoLiveIndexOutOfRange();
	Passed();
	return 0;
}
