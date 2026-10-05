#include "map_survey.h"
#include "zone_transitions.h"

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
	// Where there is left to go, before who and what is there: a player with
	// no map needs the shape of the level before its contents.
	EntityRadar::Category::UnexploredRoom,
	EntityRadar::Category::Room,
	EntityRadar::Category::Npc,
	EntityRadar::Category::Player,
	EntityRadar::Category::Corpse,
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
// been shown to be gone -- it is simply out of view. So is one further from
// the player than `within`, where the game may hold the map but not the thing.
bool Observed(const Navigation::Grid& grid, Radar::Position position, double within) {
	const double dx = position.x - grid.playerX, dy = position.y - grid.playerY;
	if (dx * dx + dy * dy > within * within) return false;
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
	const Navigation::Grid& grid, const Settings& settings,
	const std::vector<EntityRadar::Entity>* present) {
	if (level != m_level) {
		Clear();
		m_level = level;
	}

	const auto& currentlyPresent = present ? *present : visible;
	const auto loaded = [&currentlyPresent](const EntityRadar::Entity& known) {
		return std::any_of(currentlyPresent.begin(), currentlyPresent.end(), [&known](const EntityRadar::Entity& entity) {
			return EntityRadar::SameTarget(known, entity);
		});
	};
	for (const EntityRadar::Entity& entity : visible) {
		auto found = std::find_if(m_tracked.begin(), m_tracked.end(), [&entity](const Tracked& tracked) {
			return EntityRadar::SameTarget(tracked.entity, entity);
		});
		// A thing made again under a new id takes its old entry's place, rather
		// than being listed twice with the old one unreachable. One not loaded
		// is taken at most once, so two things alike stay two.
		if (found == m_tracked.end())
			found = std::find_if(m_tracked.begin(), m_tracked.end(), [&entity, &loaded](const Tracked& tracked) {
				return EntityRadar::Recreated(tracked.entity, entity) && !loaded(tracked.entity);
			});
		if (found == m_tracked.end()) {
			m_tracked.push_back({entity, 0});
		} else {
			auto refreshed = entity;
			ZoneTransitions::RestoreCrossing(refreshed, found->entity);
			found->entity = std::move(refreshed);
			found->misses = 0;
		}
	}

	// Anything not in this frame is only a miss when the map can currently see
	// where it was. Out-of-window objects keep their place indefinitely, which
	// is the point: they are what the player still has to walk to.
	for (Tracked& tracked : m_tracked) {
		const bool stillPresent = loaded(tracked.entity);
		if (!stillPresent && Observed(grid, tracked.entity.position, settings.forgetWithin)) ++tracked.misses;
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

void Registry::KeepLandmarks() {
	std::erase_if(m_tracked, [](const Tracked& tracked) {
		return tracked.entity.kind != EntityRadar::Kind::Object && tracked.entity.kind != EntityRadar::Kind::Exit;
	});
	m_entities.clear();
	for (const Tracked& tracked : m_tracked) m_entities.push_back(tracked.entity);
}

void Memory::Select(int level, bool exploration) {
	if (level == m_level) return;
	// Full assistance keeps every level's discoveries whole for the session, to
	// be walked back to (the user, 2026-09-28: "if it has been discovered and
	// loaded, I should be able to return to it using the scanner").
	if (exploration)
		if (const auto previous = m_levels.find(m_level); previous != m_levels.end()) previous->second.KeepLandmarks();
	m_level = level;
}

void Memory::Observe(int level, const std::vector<EntityRadar::Entity>& visible,
	const Navigation::Grid& grid, bool exploration, const Settings& settings,
	const std::vector<EntityRadar::Entity>* present) {
	Select(level, exploration);
	m_levels[level].Observe(level, visible, grid, settings, present);
}

const std::vector<EntityRadar::Entity>& Memory::Entities() const {
	static const std::vector<EntityRadar::Entity> Empty;
	const auto found = m_levels.find(m_level);
	return found == m_levels.end() ? Empty : found->second.Entities();
}

void Memory::Clear() {
	m_levels.clear();
	m_level = 0;
}

std::vector<Landmark> Survey(const Radar::World& world, const Navigation::Grid& grid,
	const std::vector<EntityRadar::Entity>& entities, bool fullAssistance) {
	std::vector<Landmark> result;
	result.reserve(entities.size());
	for (const EntityRadar::Entity& entity : entities) {
		Landmark landmark;
		landmark.entity       = entity;
		landmark.steps        = fullAssistance ? StepsTo(grid, entity.position) : -1;
		landmark.reachable    = landmark.steps >= 0;
		landmark.straightLine = StraightLine(world.player, entity.position);
		landmark.direction    = Radar::ScreenSector({entity.position.x - world.player.x, entity.position.y - world.player.y});
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

bool KnownLine(const Radar::World& world, Radar::Position target, double distance) {
	constexpr double SampleStep = 0.5; // match LineOfSight's half-subtile trace
	for (double depth = 0; depth <= distance; depth += SampleStep) {
		const double share = distance > 0 ? depth / distance : 0;
		const int x = Round(world.player.x + (target.x - world.player.x) * share);
		const int y = Round(world.player.y + (target.y - world.player.y) * share);
		if (world.Cell(x, y, Radar::Layer::Terrain) == Radar::State::Unknown) return false;
	}
	return world.Cell(Round(target.x), Round(target.y), Radar::Layer::Terrain) != Radar::State::Unknown;
}

Scene Split(std::vector<EntityRadar::Entity> entities) {
	Scene scene;
	for (auto& entity : entities) {
		if (entity.category == EntityRadar::Category::Enemy) scene.enemies.push_back(std::move(entity));
		else if (entity.category == EntityRadar::Category::Player) scene.players.push_back(std::move(entity));
		else if (entity.category == EntityRadar::Category::Corpse) scene.corpses.push_back(std::move(entity));
		else if (entity.kind == EntityRadar::Kind::Npc && entity.placeId) scene.townNpcs.push_back(std::move(entity));
		else if (entity.category != EntityRadar::Category::Item) scene.landmarks.push_back(std::move(entity));
	}
	return scene;
}

bool InView(const Radar::World& world, const EntityRadar::Entity& entity, double range) {
	const double distance = StraightLine(world.player, entity.position);
	return std::isfinite(distance) && distance <= range && KnownLine(world, entity.position, distance)
		&& EntityRadar::LineOfSight(world, world.player, entity);
}

std::vector<EntityRadar::Entity> Discoverable(const Radar::World& world,
	const std::vector<EntityRadar::Entity>& entities, double range) {
	std::vector<EntityRadar::Entity> result;
	for (const auto& entity : entities)
		if (InView(world, entity, range)) result.push_back(entity);
	return result;
}

bool Fixed(const EntityRadar::Entity& entity) {
	using EntityRadar::Category;
	using EntityRadar::Kind;
	if (!entity.placeId || entity.border == EntityRadar::Entity::Border::Bearing) return false;
	return (entity.kind == Kind::Exit && (entity.category == Category::Exit || entity.category == Category::Waypoint))
		|| (entity.kind == Kind::Object && entity.category == Category::Stash);
}

std::vector<EntityRadar::Entity> Charted(const Radar::World& world,
	const std::vector<EntityRadar::Entity>& entities, const std::vector<NativeRooms::Room>& charted) {
	std::vector<EntityRadar::Entity> result;
	for (const auto& entity : entities)
		if (NativeRooms::Containing(charted, entity.position) != NativeRooms::None
			|| InView(world, entity, EntityRadar::Settings{}.range)) result.push_back(entity);
	return result;
}

void TownNpcSightings::Found(int level, const std::vector<EntityRadar::Entity>& found) {
	for (const auto& npc : found) {
		if (!npc.placeId) continue;
		auto& seen = m_seen[level];
		const auto old = seen.find(npc.placeId);
		// A spawn is useful until the character has actually been located.
		// It cannot move an observed character back to their initial position.
		if (old == seen.end() || npc.unit != EntityRadar::Entity::NoUnit
			|| old->second.unit == EntityRadar::Entity::NoUnit) seen[npc.placeId] = npc;
	}
}

std::vector<EntityRadar::Entity> TownNpcSightings::Seen(int level, const std::vector<EntityRadar::Entity>& townNpcs) {
	std::vector<EntityRadar::Entity> result;
	const auto seen = m_seen.find(level);
	if (seen == m_seen.end()) return result;
	for (auto& [identity, remembered] : seen->second) {
		const auto live = std::find_if(townNpcs.begin(), townNpcs.end(), [&](const auto& npc) {
			return npc.placeId == identity && npc.unit != EntityRadar::Entity::NoUnit;
		});
		// Refresh already discovered characters even if their new location is
		// outside the discovery view; do not admit an undiscovered character.
		if (live != townNpcs.end()) remembered = *live;
		else if (const auto fallback = std::find_if(townNpcs.begin(), townNpcs.end(), [&](const auto& npc) { return npc.placeId == identity; }); fallback != townNpcs.end())
			remembered.npcApproaches = fallback->npcApproaches;
		auto listed = remembered;
		if (live == townNpcs.end()) listed.unit = EntityRadar::Entity::NoUnit;
		result.push_back(std::move(listed));
	}
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
