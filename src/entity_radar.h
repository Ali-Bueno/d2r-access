#pragma once

#include "radar.h"
#include "speech_strings.h"

#include <cstdint>
#include <string>
#include <vector>

// Directional entity scan driven by the right thumbstick: one push names the
// nearest visible NPC / enemy / object / item in that screen direction with its
// distance; pushing the same direction again cycles to the next nearest and
// wraps around. Pure logic over the collision World the sonar already reads,
// so it is unit-tested without the game.
namespace D2RAccess::EntityRadar {
// Exit: anything that takes the player to another area (level exits, portals, waypoints).
enum class Kind { Npc, Enemy, Object, Item, Exit };
struct Entity {
	uint32_t id = 0;
	Kind kind = Kind::Item;
	Radar::Position position; // collision subtiles, same frame as World::player
	std::string name; // already localized by the game's own string tables
	uint64_t levelExitId = 0; // fixed stairs/level passage: coordinate identity, independent of loaded unit id
};
bool SameTarget(const Entity& a, const Entity& b);
struct Settings {
	// Distances are collision subtiles. The spoken unit assumes one subtile
	// per game yard (the velocity unit: walk 6, run 9 subtiles per second, and
	// a legacy 800x600 screen spans about forty subtiles). Provisional until
	// calibrated live against a walked reference; keep both knobs together.
	double metersPerSubtile = 1;
	double range = 15; // subtiles; the user asked for roughly fifteen metres
	// Each direction owns a quarter of the plane: a 90-degree sector centred
	// on the fixed screen axis. Diagonal targets belong to both neighbours.
	double sectorCosine = Radar::Diagonal;
	uint64_t cycleTimeoutMs = 5000; // a later push in the same direction starts over
};
struct Target { Entity entity; double distance = 0; };
struct Result {
	bool found = false;
	Radar::Direction direction = Radar::Direction::Up;
	Target target;
	size_t index = 0, count = 0; // index is zero-based within count
};

// Terrain-layer point ray between cell centres. The origin and destination
// cells are excluded: the destination may carry the entity's own collision.
// Unknown cells never block; only positive wall evidence hides an entity.
bool LineOfSight(const Radar::World& world, Radar::Position from, Radar::Position to);
bool InSector(Radar::Position player, Radar::Position target, Radar::Direction direction, double sectorCosine = Radar::Diagonal);
// Ordinary visible entities within range; fixed level exits bypass range/walls.
// All targets obey the sector and sort nearest first with stable identity ties.
std::vector<Target> Scan(const Radar::World& world, const std::vector<Entity>& entities,
	Radar::Direction direction, const Settings& settings);

class Cycler {
public:
	// Announces targets[0] on a fresh push; a repeated push in the same
	// direction within the timeout continues after the last spoken entity
	// (looked up by id, so a rescanned list keeps its place) and wraps.
	Result Push(const std::vector<Target>& targets, Radar::Direction direction, uint64_t now, const Settings& settings);
	void Reset();
private:
	bool active_ = false;
	Radar::Direction direction_ = Radar::Direction::Up;
	uint32_t lastId_ = 0;
	Kind lastKind_ = Kind::Item;
	uint64_t lastExitId_ = 0;
	uint64_t lastPush_ = 0;
};

std::string Describe(const Result& result, const Settings& settings, const Strings::Table& strings);
// "8 metros" / "1 metro" / "aquí mismo" for a distance in subtiles; shared
// with every feature that speaks an entity distance.
std::string DistancePhrase(double distance, const Settings& settings, const Strings::Table& strings);
}
