#pragma once

#include "generated/door_sounds.h"
#include "radar.h"
#include "speech_strings.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

// Shared entity types (NPC/enemy/object/item/exit/room/corpse) plus the
// line-of-sight and spoken-distance helpers used by the level catalogue, NPC
// proximity, target sight and the beacon. Pure logic over the collision World
// the sonar already reads, so it is unit-tested without the game.
namespace D2RAccess::EntityRadar {
// Exit: anything that takes the player to another area (level exits, portals, waypoints).
// Corpse is a dead player unit, separate from Object even when numeric IDs overlap.
// Room is not a game unit: a fixed place in the level's rooms, such as a
// doorway, that guidance can lead to and never needs to re-read.
// Player is another living player in the same game (multiplayer).
enum class Kind { Npc, Enemy, Object, Item, Exit, Room, Corpse, Player };

// What the thing is FOR, which is how a player asks for it ("where is the next
// shrine", "any chests left"). Kind is too coarse for that: it files every
// shrine, chest, lever and stash alike as Object. Assigned where the object's
// table row is already in hand, so the map survey does not re-derive it.
// Interactable is the deliberate catch-all: levers, statues and doors carry no
// SubClass bit of their own. Quest takes any object a quest still needs used.
enum class Category : uint8_t {
	Unknown, Enemy, Npc, Exit, Portal, Waypoint, Shrine, Treasure, Stash, Interactable, Item, Room, Player,
	UnexploredRoom, // a room never stood in, kept apart from Room so the scanner can list where to explore
	Corpse, // a player's body, own or another's, listed from anywhere on its level
	Quest // a quest object still to be used (QuestSource), whatever else it is
};

struct EnemyTraits {
	bool important = false;
	bool resurrector = false;
	bool ranged = false;
	// What the game shows about the monster beyond its name, spoken after it:
	// unique, champion, minion, and the enchantments under its health plate.
	// Empty for an ordinary monster (monster_plate.h).
	std::string plate;
	// MonsterData's type flags (+0x1A) and enchantment slots (+0x20) as read, for the log.
	uint16_t flags = 0;
	std::array<uint8_t, 9> enchantments{};
	bool boss = false; // act/quest boss or superunique; independent of elite cue rank
};

struct Entity {
	uint32_t id = 0;
	Kind kind = Kind::Item;
	Radar::Position position; // collision subtiles, same frame as World::player
	std::string name; // already localized by the game's own string tables
	// A fixed place -- stairs, level passage, waypoint, stash -- identified by
	// where it is (NativeLevelExits::Identity), independent of the loaded unit
	// id, which changes when the unit is recreated and in every game.
	uint64_t placeId = 0;
	// A world object the player opens for loot: chest, casket, urn, barrel,
	// scenery corpse. Taken from the objects.txt interaction handler, so it holds
	// in every language, unlike the localized name. Only the chest loop reads it;
	// player corpses use Kind::Corpse and never set this flag.
	bool container = false;
	// Last so the existing positional initialisers keep working untouched.
	Category category = Category::Unknown;
	uint32_t destinationLevel = 0; // Kind::Exit: the level it leads to, 0 when unknown
	EnemyTraits enemy;
	bool questObject = false; // unused quest item source or unactivated quest mechanism
	uint8_t cairnStone = 0; // class index + 1, or zero for other entities
	std::string worldName; // original HUD label before optional guidance wording
	struct ObjectFootprint { uint32_t width = 0, height = 0; } footprint; // native collision rectangle; zero when unknown
	std::string nameKey; // the game string key `name` was read from; empty when composed or unknown
	bool corpse = false; // a container that is a dead body: it loops the corpse cue, not the chest one
	bool weaponRack = false; // a container that is a weapon rack: it loops its own cue, not the chest one
	bool unreadTome = false; // a quest book not yet read (the Moldy Tome, Horazon's journal)
	// A door still shut, by the sound the game plays when it opens; None once it is open.
	Generated::DoorSound shutDoor = Generated::DoorSound::None;
	uint32_t doorSpan = 0; // a shut door's long side in cells (objects.txt SizeX/SizeY), zero otherwise; not a footprint
	// The game's own unit type of a loaded unit (1 monster, 2 object, 5 warp tile),
	// which its commands name a target by together with `id`; NoUnit for what is
	// not one: a preset of a room not loaded yet, an area border, a room.
	static constexpr uint8_t NoUnit = 0xFF;
	uint8_t unit = NoUnit;
	// A teleport pad's objects.txt class, zero for anything else. A pad leads to
	// the other pad of its class in its room (EntityLayout::TeleportPadOperateFn).
	uint16_t padClass = 0;
};
bool SameTarget(const Entity& a, const Entity& b);
// `loaded` is `known` made again under a new unit id: the game gives an object a
// new id when its room is loaded again (MEASURED 2026-09-28: the Refilling Shrine
// at 5427,4812 was id 28, then 61; a Loose Rock 30, then 63), and an item too
// (MEASURED 10:40: five "Large Axe" and two "Bardiche, magic" listed on one spot).
// Objects and items never move: the same name within a step is it. Whether a
// monster keeps its id is NOT measured; one of the same name within the scan's
// range is taken for it, which changes nothing while ids are kept.
bool Recreated(const Entity& known, const Entity& loaded);
// The name to say, followed by an enemy's plate when it has one.
std::string SpokenName(const Entity& entity, std::string_view separator);
// A quest object still to be used: the sources of the quest and tome loops.
inline bool QuestSource(const Entity& entity) { return entity.kind == Kind::Object && (entity.questObject || entity.unreadTome); }
struct Settings {
	// Distances are collision subtiles, and a subtile is spoken as one STEP.
	// That is the unit the pathfinder already counts in -- Navigation::Grid
	// reports the shortest count of the game's own neighbouring-destination
	// steps -- so the catalogue and this scan name the same thing the same
	// way, and neither has to claim a real-world distance it never measured.
	double stepsPerSubtile = 1;
	double range = 15; // subtiles; the user asked for roughly fifteen steps
};

// Terrain-layer point ray between cell centres. The origin and destination
// cells are excluded: the destination may carry the entity's own collision.
// Unknown cells never block; only positive wall evidence hides an entity.
bool LineOfSight(const Radar::World& world, Radar::Position from, Radar::Position to);
// An object's own OBJECT bits within its footprint do not hide it; other barriers still do.
bool LineOfSight(const Radar::World& world, Radar::Position from, const Entity& target);
// "8 pasos" / "1 paso" / "aquí mismo" for a distance in subtiles; shared
// with every feature that speaks an entity distance.
std::string DistancePhrase(double distance, const Settings& settings, const Strings::Table& strings);
}
