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
// Corpse is a dead player unit, separate from Object even when numeric IDs overlap.
// Room is not a game unit: a fixed place in the level's rooms, such as a
// doorway, that guidance can lead to and never needs to re-read.
enum class Kind { Npc, Enemy, Object, Item, Exit, Room, Corpse };

// What the thing is FOR, which is how a player asks for it ("where is the next
// shrine", "any chests left"). Kind is too coarse for that: it files every
// shrine, chest, lever and stash alike as Object. Assigned where the object's
// table row is already in hand, so the map survey does not re-derive it.
// Interactable is the deliberate catch-all: levers, seals, statues and doors
// carry no SubClass bit of their own.
enum class Category : uint8_t {
	Unknown, Enemy, Npc, Exit, Portal, Waypoint, Shrine, Treasure, Stash, Interactable, Item
};

struct EnemyTraits {
	bool important = false;
	bool resurrector = false;
	bool ranged = false;
};

struct Entity {
	uint32_t id = 0;
	Kind kind = Kind::Item;
	Radar::Position position; // collision subtiles, same frame as World::player
	std::string name; // already localized by the game's own string tables
	uint64_t levelExitId = 0; // fixed stairs/level passage: coordinate identity, independent of loaded unit id
	// A world object the player opens for loot: chest, casket, urn, barrel,
	// scenery corpse. Taken from the objects.txt interaction handler, so it holds
	// in every language, unlike the localized name. Only the chest loop reads it;
	// player corpses use Kind::Corpse and never set this flag.
	bool container = false;
	// Last so the existing positional initialisers keep working untouched.
	Category category = Category::Unknown;
	uint32_t destinationLevel = 0; // Kind::Exit: the level it leads to, 0 when unknown
	EnemyTraits enemy;
};
bool SameTarget(const Entity& a, const Entity& b);
struct Settings {
	// Distances are collision subtiles, and a subtile is spoken as one STEP.
	// That is the unit the pathfinder already counts in -- Navigation::Grid
	// reports the shortest count of the game's own neighbouring-destination
	// steps -- so the catalogue and this scan name the same thing the same
	// way, and neither has to claim a real-world distance it never measured.
	double stepsPerSubtile = 1;
	double range = 15; // subtiles; the user asked for roughly fifteen steps
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
// Entities within range; ordinary ones must also be in sight, while fixed level
// exits are named through walls, since an entrance can stand in wall cells.
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
// "8 pasos" / "1 paso" / "aquí mismo" for a distance in subtiles; shared
// with every feature that speaks an entity distance.
std::string DistancePhrase(double distance, const Settings& settings, const Strings::Table& strings);
}
