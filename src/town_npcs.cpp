#include "town_npcs.h"

#include <algorithm>

namespace D2RAccess::TownNpcs {
uint64_t Identity(uint32_t monster) { return static_cast<uint64_t>(monster) + 1; }

void Merge(const std::vector<Spawn>& spawns, const std::vector<Live>& live, const NameOf& name,
	std::vector<EntityRadar::Entity>& entities) {
	for (const auto& spawn : spawns) {
		// Several presets of one row (guards) have no single place to name.
		if (std::count_if(spawns.begin(), spawns.end(), [&](const Spawn& other) { return other.monster == spawn.monster; }) != 1) continue;
		auto npcName = name(spawn.monster);
		if (npcName.empty()) continue;
		const uint64_t identity = Identity(spawn.monster);
		bool loaded = false;
		for (const auto& [index, monster] : live)
			if (monster == spawn.monster && index < entities.size()) entities[index].placeId = identity, loaded = true;
		if (loaded || spawn.aroundPlayer) continue;
		entities.push_back({static_cast<uint32_t>(identity), EntityRadar::Kind::Npc, spawn.position, std::move(npcName), identity,
			false, EntityRadar::Category::Npc});
	}
}
}
