#include "entity_radar.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace D2RAccess::EntityRadar {
namespace {
int Cell(double coordinate) { return static_cast<int>(std::lround(coordinate)); }
size_t Index(Radar::Direction direction) { return static_cast<size_t>(direction); }
}

bool SameTarget(const Entity& a, const Entity& b) {
	if (a.kind != b.kind || a.levelExitId != b.levelExitId) return false;
	return a.levelExitId || a.id == b.id;
}

bool LineOfSight(const Radar::World& world, Radar::Position from, Radar::Position to) {
	const double dx = to.x - from.x, dy = to.y - from.y;
	const double length = std::hypot(dx, dy);
	if (length == 0) return true;
	const int fromX = Cell(from.x), fromY = Cell(from.y), toX = Cell(to.x), toY = Cell(to.y);
	// Half-subtile steps visit every cell the ray crosses, including diagonal
	// corner crossings.
	constexpr double Step = 0.5;
	for (double depth = Step; depth < length; depth += Step) {
		const int x = Cell(from.x + dx / length * depth), y = Cell(from.y + dy / length * depth);
		if ((x == fromX && y == fromY) || (x == toX && y == toY)) continue;
		if (world.Cell(x, y, Radar::Layer::Terrain) == Radar::State::Blocked) return false;
	}
	return true;
}

bool InSector(Radar::Position player, Radar::Position target, Radar::Direction direction, double sectorCosine) {
	const double dx = target.x - player.x, dy = target.y - player.y;
	const double length = std::hypot(dx, dy);
	if (length == 0) return true; // sharing the player's cell: reachable from every direction
	const auto& axis = Radar::Axes[Index(direction)];
	return (dx * axis.x + dy * axis.y) / length >= sectorCosine;
}

std::vector<Target> Scan(const Radar::World& world, const std::vector<Entity>& entities,
	Radar::Direction direction, const Settings& settings) {
	std::vector<Target> targets;
	for (const auto& entity : entities) {
		const double distance = std::hypot(entity.position.x - world.player.x, entity.position.y - world.player.y);
		const bool levelExit = entity.kind == Kind::Exit && entity.levelExitId != 0;
		if (!std::isfinite(distance) || (!levelExit && distance > settings.range)) continue;
		if (!InSector(world.player, entity.position, direction, settings.sectorCosine)) continue;
		if (!levelExit && !LineOfSight(world, world.player, entity.position)) continue;
		targets.push_back({entity, distance});
	}
	std::sort(targets.begin(), targets.end(), [](const Target& a, const Target& b) {
		if (a.distance != b.distance) return a.distance < b.distance;
		if (a.entity.kind != b.entity.kind) return a.entity.kind < b.entity.kind;
		if (a.entity.levelExitId != b.entity.levelExitId) return a.entity.levelExitId < b.entity.levelExitId;
		return a.entity.id < b.entity.id;
	});
	return targets;
}

Result Cycler::Push(const std::vector<Target>& targets, Radar::Direction direction, uint64_t now, const Settings& settings) {
	Result result;
	result.direction = direction;
	result.count = targets.size();
	const bool continuing = active_ && direction == direction_ && now - lastPush_ <= settings.cycleTimeoutMs;
	direction_ = direction;
	lastPush_ = now;
	if (targets.empty()) { active_ = false; return result; }
	size_t index = 0;
	if (continuing) {
		const auto previous = std::find_if(targets.begin(), targets.end(),
			[this](const Target& target) {
				return target.entity.kind == lastKind_ && target.entity.levelExitId == lastExitId_
					&& (lastExitId_ || target.entity.id == lastId_);
			});
		if (previous != targets.end()) index = (static_cast<size_t>(previous - targets.begin()) + 1) % targets.size();
	}
	result.found = true;
	result.index = index;
	result.target = targets[index];
	active_ = true;
	lastId_ = result.target.entity.id;
	lastKind_ = result.target.entity.kind;
	lastExitId_ = result.target.entity.levelExitId;
	return result;
}

void Cycler::Reset() { *this = Cycler(); }

std::string Describe(const Result& result, const Settings& settings, const Strings::Table& strings) {
	char buffer[128];
	const char* direction = strings.scanDirections[Index(result.direction)];
	if (!result.found) {
		std::snprintf(buffer, sizeof buffer, strings.scanNothing, direction);
		return buffer;
	}
	// The name and how far, and nothing else. A position in the candidate
	// list is heard on every single push, and the player did not ask for it.
	return result.target.entity.name + ", " + DistancePhrase(result.target.distance, settings, strings);
}

std::string DistancePhrase(double distance, const Settings& settings, const Strings::Table& strings) {
	const long steps = std::lround(distance * settings.stepsPerSubtile);
	if (steps < 1) return strings.scanAdjacent;
	if (steps == 1) return strings.scanStep;
	char buffer[64];
	std::snprintf(buffer, sizeof buffer, strings.scanSteps, static_cast<int>(steps));
	return buffer;
}
}
