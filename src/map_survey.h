#pragma once

#include "entity_radar.h"
#include "native_rooms.h"
#include "navigation_grid.h"
#include "radar.h"

#include <cstdint>
#include <map>
#include <vector>

// A level-wide catalogue of what is on the map, with the WALKING distance to
// each thing rather than its straight line.
//
// The directional scan (entity_radar.h) answers "what is to my left, within
// fifteen subtiles, that I can see". That leaves the player stranded whenever
// nothing is in range or everything in range is behind a wall: the scan is
// silent and there is no way to learn where to go. This answers the other
// question -- "what is on this level at all, and how do I get to it" -- so
// there is always somewhere to head for.
//
// Distance comes from Navigation::Grid::steps, which counts steps along the
// game's own neighbouring-destination graph. That graph is what the game walks
// the player along when a tile is clicked, so the count already goes around
// obstacles instead of through them, and an unreachable object is reported as
// unreachable rather than as a straight line through a wall.
//
// Pure logic over a World and a Grid: no native calls, no SDK, unit-tested
// without the game.
namespace D2RAccess::MapSurvey {

// One catalogued object, placed relative to the player.
struct Landmark {
	EntityRadar::Entity entity;
	int                 steps        = -1; // -1 when no walkable route is known
	double              straightLine = 0;  // subtiles; ordering fallback and reporting
	int                 direction    = 0; // Radar::ScreenSector, up and diagonals included
	bool                reachable    = false;
};

struct Settings {
	// An object is only dropped once the map has had this many chances to show
	// it again, so a monster stepping behind a corner does not erase it.
	int missesBeforeForgetting = 3;
};

// What has been seen on the current level. Entities come and go as the world
// window moves, so this accumulates them instead of trusting one frame, and
// drops one only when the map can currently see its last known place and it is
// no longer there. Changing level clears everything.
class Registry {
public:
	// `visible` is what the world reports right now; `grid` says which cells
	// the map currently knows, so absence can be told from ignorance.
	void Observe(int level, const std::vector<EntityRadar::Entity>& visible,
		const Navigation::Grid& grid, const Settings& settings = {},
		const std::vector<EntityRadar::Entity>* present = nullptr);

	void Clear();
	// A return visit retains fixed places; enemies and loot are transient.
	void KeepLandmarks();

	int                                     Level() const { return m_level; }
	const std::vector<EntityRadar::Entity>& Entities() const { return m_entities; }

private:
	struct Tracked {
		EntityRadar::Entity entity;
		int                 misses = 0;
	};
	int                  m_level = 0;
	std::vector<Tracked> m_tracked;
	// Rebuilt with m_tracked so callers get a plain list.
	std::vector<EntityRadar::Entity> m_entities;
};

// Session-scoped landmark memory. Full assistance keeps the original
// level-change reset; exploration retains fixed places until Clear.
class Memory {
public:
	void Select(int level, bool exploration);
	void Observe(int level, const std::vector<EntityRadar::Entity>& visible,
		const Navigation::Grid& grid, bool exploration, const Settings& settings = {},
		const std::vector<EntityRadar::Entity>* present = nullptr);
	const std::vector<EntityRadar::Entity>& Entities() const;
	void Clear();
private:
	int m_level = 0;
	std::map<int, Registry> m_levels;
};

// Distance and direction for everything known, nearest first: walking steps
// with full assistance, otherwise the straight line alone. Unreachable objects
// sort after reachable ones, by straight line.
std::vector<Landmark> Survey(const Radar::World& world, const Navigation::Grid& grid,
	const std::vector<EntityRadar::Entity>& entities, bool fullAssistance = true);

// What the scanner lists: map landmarks the registry remembers, chests and
// every other container among them (the user, 2026-09-28), enemies and other
// players read live because they move, and player corpses, which the corpse
// memory keeps level-wide. Items on the ground are read on their own.
struct Scene { std::vector<EntityRadar::Entity> landmarks, enemies, players, corpses; };
Scene Split(std::vector<EntityRadar::Entity> entities);

// A landmark the level's layout puts in the same place in every game on the
// same map: exits (stairs, level passages, area borders), waypoints and the
// stash, each with a place identity. Shrines, chests, quest objects and
// portals are left out: what they are, or whether they are there, changes
// between games. These are kept with the map (TerrainService::RememberPlaces).
bool Fixed(const EntityRadar::Entity& entity);

// New discoveries require the scan's ordinary range and an unobstructed view.
// Fixed exits do not inherit the short scan's through-wall exception.
std::vector<EntityRadar::Entity> Discoverable(const Radar::World& world,
	const std::vector<EntityRadar::Entity>& entities,
	double range = EntityRadar::Settings{}.range);
// Discoverable, plus anything standing in `charted` rooms (drawn on the automap).
std::vector<EntityRadar::Entity> Charted(const Radar::World& world,
	const std::vector<EntityRadar::Entity>& entities, const std::vector<NativeRooms::Room>& charted);

// The nearest `count` of one kind, in Survey order.
std::vector<Landmark> OfKind(const std::vector<Landmark>& landmarks, EntityRadar::Kind kind, size_t count);

// Everything of one category, in Survey order (nearest on foot first).
std::vector<Landmark> OfCategory(const std::vector<Landmark>& landmarks, EntityRadar::Category category);

// The categories actually present, in a fixed order that puts what a stranded
// player most needs -- a way out, then a way onward -- before the scenery.
std::vector<EntityRadar::Category> Categories(const std::vector<Landmark>& landmarks);

// Step from `category` to the next or previous one that has anything in it,
// wrapping. Returns the first present category when `category` has none.
EntityRadar::Category StepCategory(const std::vector<Landmark>& landmarks, EntityRadar::Category category, int delta);

}
