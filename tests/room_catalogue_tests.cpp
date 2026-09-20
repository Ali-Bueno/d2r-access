#include "room_catalogue.h"
#include "test_support.h"
#include <vector>

using namespace D2RAccess;
namespace {
NativeRooms::Room Room(int tileX, int tileY, int width = 8, int height = 8) {
	return {tileX, tileY, width, height, NativeRooms::Identity(tileX, tileY)};
}
// Two rooms side by side: room 1 spans subtiles [0,40), room 2 spans [40,80).
const std::vector<NativeRooms::Room> Level{Room(0, 0), Room(8, 0)};
// Same count, different rectangles: a level regenerated under the same id.
const std::vector<NativeRooms::Room> Regenerated{Room(0, 0), Room(8, 8)};
constexpr uint32_t Seed = 0x6AF97A27, OtherSeed = 0x52E91755;
}

int main() {
	RoomCatalogue::Registry registry;
	registry.Observe(5, Seed, Level, {5, 5});
	Check(registry.Entries().size() == 2, "every room of the level is catalogued, entered or not");
	Check(registry.Entries()[0].number == 1 && registry.Entries()[1].number == 2, "rooms are numbered from one in the order they are read");
	Check(registry.Entries()[0].explored && !registry.Entries()[1].explored, "standing in a room explores it and only it");
	Check(registry.Entries()[0].centre.x == 19.5 && registry.Entries()[0].centre.y == 19.5, "the centre is the middle of the subtile rectangle");
	Check(registry.MapSeed() == Seed, "the registry reports the seed it is keyed on");

	registry.Observe(5, Seed, Level, {45, 5});
	Check(registry.Entries()[0].explored && registry.Entries()[1].explored, "exploration accumulates instead of following the player");
	registry.Observe(5, Seed, Level, {500, 500});
	Check(registry.Entries()[0].explored, "a position in no room leaves the record alone");

	// Leaving a level and coming back, in any game: the layout is a function of
	// the seed and the level id, so the record still describes these rooms.
	registry.Observe(6, Seed, Level, {45, 5});
	Check(!registry.Entries()[0].explored && registry.Entries()[1].explored, "another level has its own record");
	registry.Observe(5, Seed, Level, {500, 500});
	Check(registry.Entries()[0].explored && registry.Entries()[1].explored, "returning to a level restores what was explored there");
	Check(registry.Remembered() == 1, "the level being displayed is held live, not twice");

	// The game session is NOT part of the identity: a new game with the same
	// character regenerates nothing, so the record has to survive it.
	registry.Observe(5, Seed, Level, {500, 500});
	Check(registry.Entries()[0].explored && registry.Entries()[1].explored, "a new game keeps the record, because the dungeon is the same one");

	// A different character, or an online game: a different seed is a
	// different dungeon even at the same level id.
	registry.Observe(5, OtherSeed, Level, {500, 500});
	Check(!registry.Entries()[0].explored && !registry.Entries()[1].explored, "a different map seed is a different dungeon");
	registry.Observe(5, Seed, Level, {500, 500});
	Check(registry.Entries()[0].explored && registry.Entries()[1].explored, "and the original character's record is still there");

	// Without a readable seed the rectangles alone still decide, so a
	// regenerated level cannot inherit the old numbering.
	registry.Observe(9, 0, Level, {5, 5});
	Check(registry.Entries()[0].explored && !registry.Entries()[1].explored, "a level with no seed is still catalogued");
	registry.Observe(9, 0, Regenerated, {500, 500});
	Check(!registry.Entries()[0].explored && !registry.Entries()[1].explored, "rooms in different places are a different layout, seed or no seed");
	registry.Observe(9, 0, Level, {500, 500});
	Check(registry.Entries()[0].explored, "and the first layout is remembered under its own rectangles");

	// Names and identity.
	registry.Observe(7, Seed, Level, {5, 5});
	const auto entities = RoomCatalogue::Entities(registry.Entries(), "room %d", "unexplored room %d");
	Check(entities.size() == 2, "one catalogue entity per room");
	Check(entities[0].name == "room 1", "an explored room is named by its number");
	Check(entities[1].name == "unexplored room 2", "a room never entered says so, and still says where it is");
	Check(entities[0].category == EntityRadar::Category::Room && entities[0].kind == EntityRadar::Kind::Room, "rooms are their own kind and category");
	Check(entities[0].levelExitId == Level[0].id && entities[0].levelExitId != entities[1].levelExitId, "identity is the room's rectangle, so it survives a rebuilt list");
	Check(entities[0].position.x == 19.5, "the entity sits at the room centre for bearing and ordering");
	Check(!EntityRadar::SameTarget(entities[0], entities[1]), "two rooms are never the same target");

	// A monster and a room can share a numeric id without ever colliding.
	EntityRadar::Entity monster;
	monster.id = entities[0].id;
	monster.kind = EntityRadar::Kind::Enemy;
	Check(!EntityRadar::SameTarget(entities[0], monster), "a room lock cannot follow a unit that happens to share its number");

	// The remembered set is bounded, and bounded by dropping the oldest.
	RoomCatalogue::Registry bounded;
	for (size_t i = 0; i < RoomCatalogue::MaxRemembered + 8; ++i)
		bounded.Observe(static_cast<int>(i) + 1, Seed, Level, {5, 5});
	Check(bounded.Remembered() <= RoomCatalogue::MaxRemembered, "room memory does not grow without limit");

	registry.Clear();
	Check(registry.Entries().empty() && registry.Level() == 0 && registry.Remembered() == 0, "clearing forgets every level, not just this one");
	registry.Observe(8, Seed, {}, {0, 0});
	Check(registry.Entries().empty(), "a level whose rooms cannot be read simply has no rooms");
	Check(RoomCatalogue::Entities({}, "room %d", "unexplored room %d").empty(), "no rooms, no entities");
	Passed();
}
