#include "detour_service.h"

#include "keyboard_service.h"
#include "native_entities.h"
#include "speech_strings.h"

#include <algorithm>
#include <cstdio>

namespace D2RAccess::DetourService {
namespace {
// Policy: said after the area's banner, which interrupts (TerrainService's ArrivalHoldMs).
constexpr uint64_t ArrivalHoldMs = 2500;

struct Choice {
	uint64_t session = 0;
	int level = 0, via = 0; // the room's level, the level passed through
	EntityRadar::Entity room;
	NativeRooms::Bounds bounds;
};
std::vector<Choice> g_found;
std::optional<Choice> g_chosen;
int g_level = 0;
uint64_t g_arrivedAt = 0;
bool g_handled = false;

EntityRadar::Entity Crossing(const EntityRadar::Entity& room, Radar::Position at, int destination) {
	EntityRadar::Entity entity = room;
	entity.kind = EntityRadar::Kind::Exit;
	entity.category = EntityRadar::Category::UnexploredRoom;
	entity.position = at;
	entity.destinationLevel = static_cast<uint32_t>(destination);
	entity.mapLandmark = true; // a place of the map: nothing to read again
	return entity;
}
}

std::vector<EntityRadar::Entity> Find(const Radar::World& world, const std::vector<EntityRadar::Entity>& cut, bool mostlyUnseen) {
	g_found.clear();
	std::vector<EntityRadar::Entity> result;
	if (cut.empty()) return result;
	std::vector<const EntityRadar::Entity*> rooms;
	for (const auto& room : cut) rooms.push_back(&room);
	const auto any = [](const auto& ways) { return std::any_of(ways.begin(), ways.end(), [](const auto& way) { return way.has_value(); }); };
	auto ways = KeyboardService::Detours(world, rooms, false);
	if (mostlyUnseen && !any(ways)) ways = KeyboardService::Detours(world, rooms, true);
	std::vector<size_t> order;
	for (size_t i = 0; i < ways.size(); ++i) if (ways[i]) order.push_back(i);
	std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return ways[a]->steps < ways[b]->steps; });
	for (const size_t i : order) {
		const auto& way = *ways[i];
		g_found.push_back({world.session, world.level, way.via, cut[i], way.room});
		auto entity = Crossing(cut[i], way.cell, way.via);
		char name[256]{};
		std::snprintf(name, sizeof(name), Strings::Current().detourThrough, cut[i].name.c_str(),
			NativeEntities::LevelName(static_cast<uint32_t>(way.via)).c_str());
		entity.name = name;
		result.push_back(std::move(entity));
	}
	return result;
}

bool IsDetour(const EntityRadar::Entity& entity) {
	return entity.kind == EntityRadar::Kind::Exit && entity.category == EntityRadar::Category::UnexploredRoom;
}

void Chosen(const EntityRadar::Entity& entity) {
	if (!IsDetour(entity)) return;
	for (const auto& found : g_found)
		if (found.room.placeId == entity.placeId && found.via == static_cast<int>(entity.destinationLevel)) g_chosen = found;
}

std::optional<EntityRadar::Entity> Onward(const Radar::World& world) {
	if (!g_chosen || g_chosen->session != world.session || g_chosen->via != world.level) return std::nullopt;
	const auto way = KeyboardService::DetourBack(world, g_chosen->level, g_chosen->bounds);
	if (!way) return std::nullopt;
	auto entity = Crossing(g_chosen->room, way->cell, g_chosen->level);
	entity.name += Strings::Current().listSeparator + NativeEntities::LevelName(static_cast<uint32_t>(g_chosen->level));
	return entity;
}

bool Due(uint64_t session, int level, uint64_t now) {
	if (level != g_level) {
		g_level = level;
		g_arrivedAt = now;
		g_handled = false;
	}
	if (g_chosen && (g_chosen->session != session || (level != g_chosen->level && level != g_chosen->via))) g_chosen.reset();
	if (!g_chosen || g_handled || now - g_arrivedAt < ArrivalHoldMs) return false;
	g_handled = true;
	return true;
}

void ForgetReached(const Radar::World& world) {
	if (g_chosen && world.level == g_chosen->level && !KeyboardService::CutOff(world, {&g_chosen->room}).front()) g_chosen.reset();
}
}
