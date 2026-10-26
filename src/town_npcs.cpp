#include "town_npcs.h"

#include "generated/npc_markers.h"

#include <algorithm>

namespace D2RAccess::TownNpcs {
uint64_t Identity(uint32_t monster) { return static_cast<uint64_t>(monster) + 1; }

std::vector<Spawn> MarkerSpawns(const std::vector<Marker>& markers, const SpawnTest& spawns) {
	std::vector<Spawn> result;
	for (const auto& marker : markers)
		for (const auto& known : Generated::NpcMarkers)
			if (known.object == marker.object && spawns(known.initFn).value_or(false))
				result.push_back({known.monster, marker.position, marker.aroundPlayer});
	return result;
}

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
	// A loaded NPC the presets never place (Jerhyn) is identified by its row when it is the only one.
	for (const auto& [index, monster] : live) {
		if (index >= entities.size() || name(monster).empty()) continue;
		const auto sameRow = [&](const Live& other) { return other.second == monster; };
		const auto placed = [&](const Spawn& spawn) { return spawn.monster == monster; };
		if (std::count_if(live.begin(), live.end(), sameRow) == 1 && std::none_of(spawns.begin(), spawns.end(), placed))
			entities[index].placeId = Identity(monster);
	}
}
}
