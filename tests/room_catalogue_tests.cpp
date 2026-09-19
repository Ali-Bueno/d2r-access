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
}

int main() {
	RoomCatalogue::Registry registry;
	registry.Observe(5, 100, Level, {5, 5});
	Check(registry.Entries().size() == 2, "every room of the level is catalogued, entered or not");
	Check(registry.Entries()[0].number == 1 && registry.Entries()[1].number == 2, "rooms are numbered from one in the order they are read");
	Check(registry.Entries()[0].explored && !registry.Entries()[1].explored, "standing in a room explores it and only it");
	Check(registry.Entries()[0].centre.x == 19.5 && registry.Entries()[0].centre.y == 19.5, "the centre is the middle of the subtile rectangle");

	registry.Observe(5, 100, Level, {45, 5});
	Check(registry.Entries()[0].explored && registry.Entries()[1].explored, "exploration accumulates instead of following the player");
	registry.Observe(5, 100, Level, {500, 500});
	Check(registry.Entries()[0].explored, "a position in no room leaves the record alone");

	registry.Observe(6, 100, Level, {45, 5});
	Check(!registry.Entries()[0].explored && registry.Entries()[1].explored, "a new level starts the exploration over");
	registry.Observe(6, 101, Level, {5, 5});
	Check(registry.Entries()[0].explored && !registry.Entries()[1].explored, "a new game session starts it over too, because the level regenerated");

	// Names and identity.
	registry.Observe(7, 1, Level, {5, 5});
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

	registry.Clear();
	Check(registry.Entries().empty() && registry.Level() == 0, "clearing forgets the level");
	registry.Observe(8, 1, {}, {0, 0});
	Check(registry.Entries().empty(), "a level whose rooms cannot be read simply has no rooms");
	Check(RoomCatalogue::Entities({}, "room %d", "unexplored room %d").empty(), "no rooms, no entities");
	Passed();
}
