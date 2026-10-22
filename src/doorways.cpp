#include "doorways.h"

#include "generated/level_flags.h"
#include "level_exit_layout.h"
#include "native_level_exits.h"
#include "place_service.h"
#include "speech_strings.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::Doorways {
namespace {
// LvlTypes.txt rows whose halls open into each other through doorways: "Act 1 - Crypt" (the
// Crypt, the Mausoleum, the Tower Cellar) and "Act 1 - Tristram" (a tester, 2026-10-22: its houses).
constexpr int32_t CryptLevelType = 4, TristramLevelType = 11;

bool Everywhere(int32_t levelType) { return levelType == CryptLevelType || levelType == TristramLevelType; }

bool Inside(const Box& box, Radar::Position at) {
	return at.x >= box.minX && at.x < box.maxX && at.y >= box.minY && at.y < box.maxY;
}
}

std::vector<Rooms::Doorway> Select(const std::vector<Rooms::Doorway>& found, int32_t levelType,
	const std::vector<Box>& buildings, const std::vector<Radar::Position>& doors) {
	std::vector<Rooms::Doorway> result;
	for (const auto& doorway : found) {
		if (!Everywhere(levelType)
			&& std::none_of(buildings.begin(), buildings.end(), [&](const Box& box) { return Inside(box, doorway.centre); })) continue;
		// A door object's own gap has its door's sound.
		if (std::any_of(doors.begin(), doors.end(), [&](Radar::Position door) {
				return std::abs(door.x - doorway.centre.x) + std::abs(door.y - doorway.centre.y) <= LevelExitLayout::SubtilesPerTile; }))
			continue;
		result.push_back(doorway);
	}
	return result;
}

std::vector<Rooms::Doorway> Find(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities) {
	const int32_t levelType = Generated::FindLevelFlags(world.level).levelType;
	const auto buildings = Everywhere(levelType) ? std::vector<Box>{} : PlaceService::Buildings();
	if (!Everywhere(levelType) && buildings.empty()) return {};
	std::vector<Radar::Position> doors;
	for (const auto& entity : entities)
		if (entity.kind == EntityRadar::Kind::Object
			&& (entity.shutDoor != Generated::DoorSound::None || entity.openedDoor != Generated::DoorSound::None))
			doors.push_back(entity.position);
	return Select(Rooms::Doorways(world), levelType, buildings, doors);
}

EntityRadar::Entity Listed(const Rooms::Doorway& doorway) {
	EntityRadar::Entity entity;
	entity.kind = EntityRadar::Kind::Object;
	entity.category = EntityRadar::Category::Interactable;
	entity.position = doorway.centre;
	entity.placeId = NativeLevelExits::Identity(entity.position);
	entity.name = Strings::Current().doorway;
	entity.mapLandmark = true;
	entity.doorway = true;
	return entity;
}

}
