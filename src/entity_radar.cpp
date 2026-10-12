#include "entity_radar.h"

#include <cmath>
#include <cstdio>

namespace D2RAccess::EntityRadar {
namespace {
int Cell(double coordinate) { return static_cast<int>(std::lround(coordinate)); }
bool SightBlocked(const Radar::World& world, int x, int y, Radar::Position centre,
	const Entity::ObjectFootprint& footprint) {
	uint16_t flags = 0;
	const auto state = world.Cell(x, y, Radar::Layer::Terrain, &flags);
	if (state != Radar::State::Blocked) return false;
	const int64_t left = static_cast<int64_t>(Cell(centre.x)) - footprint.width / 2;
	const int64_t top = static_cast<int64_t>(Cell(centre.y)) - footprint.height / 2;
	if (static_cast<int64_t>(x) < left || static_cast<int64_t>(x) >= left + footprint.width
		|| static_cast<int64_t>(y) < top || static_cast<int64_t>(y) >= top + footprint.height) return true;
	return (Radar::TerrainCollisionFlags(flags) & ~Radar::Collision::Object & world.walkingMask) != 0;
}
bool TraceSight(const Radar::World& world, Radar::Position from, Radar::Position to,
	const Entity::ObjectFootprint& footprint) {
	const double dx = to.x - from.x, dy = to.y - from.y;
	const double length = std::hypot(dx, dy);
	const bool sizedObject = footprint.width && footprint.height;
	const int fromX = Cell(from.x), fromY = Cell(from.y), toX = Cell(to.x), toY = Cell(to.y);
	// Half-subtile steps visit every cell the ray crosses, including diagonal
	// corner crossings.
	constexpr double Step = 0.5;
	for (double depth = Step; depth < length; depth += Step) {
		const int x = Cell(from.x + dx / length * depth), y = Cell(from.y + dy / length * depth);
		if ((x == fromX && y == fromY) || (x == toX && y == toY)) continue;
		if (SightBlocked(world, x, y, to, footprint)) return false;
	}
	return !sizedObject || !SightBlocked(world, toX, toY, to, footprint);
}
}

std::string SpokenName(const Entity& entity, std::string_view separator) {
	if (entity.enemy.plate.empty()) return entity.name;
	std::string text = entity.name;
	text += separator;
	text += entity.enemy.plate;
	return text;
}

bool SameTarget(const Entity& a, const Entity& b) {
	if (a.kind != b.kind || a.placeId != b.placeId) return false;
	return a.placeId || a.id == b.id;
}

bool Recreated(const Entity& known, const Entity& loaded) {
	if (known.kind != loaded.kind || known.placeId || loaded.placeId || known.id == loaded.id
		|| known.category != loaded.category || known.name != loaded.name) return false;
	double within = 0;
	switch (known.kind) {
	case Kind::Object: case Kind::Item: within = 1; break;
	case Kind::Enemy: case Kind::Npc: within = Settings{}.range; break;
	default: return false;
	}
	return std::abs(known.position.x - loaded.position.x) <= within && std::abs(known.position.y - loaded.position.y) <= within;
}

bool LineOfSight(const Radar::World& world, Radar::Position from, Radar::Position to) {
	return TraceSight(world, from, to, {});
}
bool LineOfSight(const Radar::World& world, Radar::Position from, const Entity& target) {
	return TraceSight(world, from, target.position, target.kind == Kind::Object ? target.footprint : Entity::ObjectFootprint{});
}

// Policy: the player's cross-shaped body covers the subtiles beside its own, so
// a thing on any of them, diagonals included, is under the player.
constexpr double AdjacentSubtiles = 1.5;

bool UnderPlayer(double distance, const Settings& settings) {
	return distance < AdjacentSubtiles || std::lround(distance * settings.stepsPerSubtile) < 1;
}

std::string DistancePhrase(double distance, const Settings& settings, const Strings::Table& strings) {
	const long steps = std::lround(distance * settings.stepsPerSubtile);
	if (UnderPlayer(distance, settings)) return strings.scanAdjacent;
	if (steps == 1) return strings.scanStep;
	char buffer[64];
	std::snprintf(buffer, sizeof buffer, strings.scanSteps, static_cast<int>(steps));
	return buffer;
}
}
