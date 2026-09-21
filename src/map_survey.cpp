#include "map_survey.h"

#include "npc_proximity.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace D2RAccess::MapSurvey {

namespace {

// Cycling order. A stranded player's first question is how to leave, so the
// ways out come first; then what the level is for, then its scenery. Items sit
// last because the ground-item reader already covers what is underfoot.
constexpr EntityRadar::Category CategoryOrder[] = {
	EntityRadar::Category::Exit,
	EntityRadar::Category::Portal,
	EntityRadar::Category::Waypoint,
	EntityRadar::Category::Npc,
	EntityRadar::Category::Stash,
	EntityRadar::Category::Enemy,
	EntityRadar::Category::Treasure,
	EntityRadar::Category::Shrine,
	EntityRadar::Category::Interactable,
	EntityRadar::Category::Item,
	EntityRadar::Category::Unknown,
};

int Round(double value) { return static_cast<int>(std::lround(value)); }

// True when the map currently has an opinion about this cell. An entity whose
// last known cell is outside the grid, or inside it but still unknown, has not
// been shown to be gone -- it is simply out of view.
bool Observed(const Navigation::Grid& grid, Radar::Position position) {
	const Navigation::Cell* cell = grid.At(Round(position.x), Round(position.y));
	return cell != nullptr && cell->body != Radar::State::Unknown;
}

double StraightLine(Radar::Position from, Radar::Position to) {
	const double dx = to.x - from.x, dy = to.y - from.y;
	return std::sqrt(dx * dx + dy * dy);
}

// Steps to the entity's own cell, or to the closest walkable cell touching it:
// a monster or a chest stands ON a blocked cell, so its own cell often has no
// route while the tile beside it does.
int StepsTo(const Navigation::Grid& grid, Radar::Position position) {
	const int x = Round(position.x), y = Round(position.y);
	int best = -1;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const Navigation::Cell* cell = grid.At(x + dx, y + dy);
			if (cell == nullptr || cell->steps < 0) continue;
			if (best < 0 || cell->steps < best) best = cell->steps;
		}
	}
	return best;
}

}

void Registry::Observe(int level, const std::vector<EntityRadar::Entity>& visible,
	const Navigation::Grid& grid, const Settings& settings) {
	if (level != m_level) {
		Clear();
		m_level = level;
	}

	for (const EntityRadar::Entity& entity : visible) {
		const auto found = std::find_if(m_tracked.begin(), m_tracked.end(), [&entity](const Tracked& tracked) {
			return EntityRadar::SameTarget(tracked.entity, entity);
		});
		if (found == m_tracked.end()) {
			m_tracked.push_back({entity, 0});
		} else {
			found->entity = entity;
			found->misses = 0;
		}
	}

	// Anything not in this frame is only a miss when the map can currently see
	// where it was. Out-of-window objects keep their place indefinitely, which
	// is the point: they are what the player still has to walk to.
	for (Tracked& tracked : m_tracked) {
		const bool present = std::any_of(visible.begin(), visible.end(), [&tracked](const EntityRadar::Entity& entity) {
			return EntityRadar::SameTarget(tracked.entity, entity);
		});
		if (!present && Observed(grid, tracked.entity.position)) ++tracked.misses;
	}
	m_tracked.erase(std::remove_if(m_tracked.begin(), m_tracked.end(), [&settings](const Tracked& tracked) {
		return tracked.misses >= settings.missesBeforeForgetting;
	}), m_tracked.end());

	m_entities.clear();
	m_entities.reserve(m_tracked.size());
	for (const Tracked& tracked : m_tracked) m_entities.push_back(tracked.entity);
}

void Registry::Clear() {
	m_tracked.clear();
	m_entities.clear();
}

std::vector<Landmark> Survey(const Radar::World& world, const Navigation::Grid& grid,
	const std::vector<EntityRadar::Entity>& entities) {
	std::vector<Landmark> result;
	result.reserve(entities.size());
	for (const EntityRadar::Entity& entity : entities) {
		Landmark landmark;
		landmark.entity       = entity;
		landmark.steps        = StepsTo(grid, entity.position);
		landmark.reachable    = landmark.steps >= 0;
		landmark.straightLine = StraightLine(world.player, entity.position);
		landmark.direction    = NpcProximity::DirectionOf(world.player, entity.position);
		result.push_back(std::move(landmark));
	}
	// Reachable first, then fewest steps. Unreachable objects keep their place
	// in the list by straight line, so they can still be named as "no route".
	std::sort(result.begin(), result.end(), [](const Landmark& a, const Landmark& b) {
		if (a.reachable != b.reachable) return a.reachable;
		if (a.reachable) {
			return std::tie(a.steps, a.straightLine, a.entity.id) < std::tie(b.steps, b.straightLine, b.entity.id);
		}
		return std::tie(a.straightLine, a.entity.id) < std::tie(b.straightLine, b.entity.id);
	});
	return result;
}

std::vector<Landmark> OfCategory(const std::vector<Landmark>& landmarks, EntityRadar::Category category) {
	std::vector<Landmark> result;
	for (const Landmark& landmark : landmarks) {
		if (landmark.entity.category == category) result.push_back(landmark);
	}
	return result;
}

std::vector<EntityRadar::Category> Categories(const std::vector<Landmark>& landmarks) {
	std::vector<EntityRadar::Category> result;
	for (EntityRadar::Category category : CategoryOrder) {
		if (std::any_of(landmarks.begin(), landmarks.end(), [category](const Landmark& landmark) {
				return landmark.entity.category == category;
			})) {
			result.push_back(category);
		}
	}
	return result;
}

EntityRadar::Category StepCategory(const std::vector<Landmark>& landmarks, EntityRadar::Category category, int delta) {
	const std::vector<EntityRadar::Category> present = Categories(landmarks);
	if (present.empty()) return EntityRadar::Category::Unknown;
	const auto found = std::find(present.begin(), present.end(), category);
	if (found == present.end()) return present.front();
	const auto size  = static_cast<int>(present.size());
	const auto index = static_cast<int>(found - present.begin());
	// Wrap in both directions without relying on C++ modulo of a negative.
	return present[static_cast<size_t>(((index + delta) % size + size) % size)];
}

std::vector<Landmark> OfKind(const std::vector<Landmark>& landmarks, EntityRadar::Kind kind, size_t count) {
	std::vector<Landmark> result;
	for (const Landmark& landmark : landmarks) {
		if (result.size() >= count) break;
		if (landmark.entity.kind == kind) result.push_back(landmark);
	}
	return result;
}

}
