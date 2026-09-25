#include "corpse_memory.h"
#include "test_support.h"
#include <vector>

using namespace D2RAccess;
namespace {
NativeRooms::Room Room(int tileX, int tileY) {
	return {tileX, tileY, 8, 8, NativeRooms::Identity(tileX, tileY)};
}
EntityRadar::Entity Corpse(uint32_t id, double x, double y) {
	EntityRadar::Entity corpse;
	corpse.id = id;
	corpse.kind = EntityRadar::Kind::Corpse;
	corpse.category = EntityRadar::Category::Corpse;
	corpse.position = {x, y};
	corpse.name = "cadáver";
	return corpse;
}
const auto Always = [](Radar::Position) { return true; };
const auto Never = [](Radar::Position) { return false; };
}

int main() {
	// A, B, C in a row; D below B, meeting A only at a corner. Subtiles are five per tile.
	const std::vector<NativeRooms::Room> rooms{Room(0, 0), Room(8, 0), Room(16, 0), Room(8, 8)};
	Navigation::Grid grid;
	grid.width = 120, grid.height = 80;
	grid.cells.resize(120 * 80);
	for (int y = 0; y < 80; ++y)
		for (int x = 0; x < 80; ++x) grid.cells[static_cast<size_t>(y) * 120 + x].body = Radar::State::Open; // C stays unloaded
	const Radar::Position inA{5, 5}, inB{45, 10};
	Check(CorpseMemory::Covered(rooms, grid, inA, {10, 10}), "the player's own loaded room is covered");
	Check(CorpseMemory::Covered(rooms, grid, inA, inB), "a loaded room touching the player's is covered");
	Check(!CorpseMemory::Covered(rooms, grid, inA, {85, 10}), "a room further off is not read, so absence there proves nothing");
	Check(!CorpseMemory::Covered(rooms, grid, inA, {45, 45}), "a room meeting the player's at a corner is not assumed read");
	Check(!CorpseMemory::Covered(rooms, grid, inB, {85, 10}), "a touching room without collision is not loaded, so not read");
	Check(!CorpseMemory::Covered(rooms, grid, inA, {500, 500}), "a place in no room is never covered");

	CorpseMemory::Memory memory;
	memory.Observe(1, {Corpse(50, 10, 10)}, Always);
	Check(memory.Entities(1).size() == 1 && memory.Entities(1)[0].id == 50, "a corpse read once is remembered");
	for (int i = 0; i < 10; ++i) memory.Observe(1, {}, Never);
	Check(memory.Entities(1).size() == 1, "out of the read's reach it stays, however long");
	memory.Observe(2, {Corpse(60, 20, 20)}, Always);
	Check(memory.Entities(1).size() == 1 && memory.Entities(2).size() == 1, "each level keeps its own corpses across a level change");
	memory.Observe(1, {}, Always);
	memory.Observe(1, {}, Always);
	memory.Observe(1, {Corpse(50, 10, 10), Corpse(51, 30, 30)}, Always);
	Check(memory.Entities(1).size() == 2, "one read that misses it is not proof it is gone, and a second corpse joins");
	memory.Observe(1, {Corpse(51, 30, 30)}, Always);
	memory.Observe(1, {Corpse(51, 30, 30)}, Always);
	Check(memory.Entities(1).size() == 2, "two covered misses still keep it");
	memory.Observe(1, {Corpse(51, 30, 30)}, Always);
	Check(memory.Entities(1).size() == 1 && memory.Entities(1)[0].id == 51, "its place seen empty three times: taken back");
	memory.Clear();
	Check(memory.Entities(1).empty() && memory.Entities(2).empty(), "a new game starts with no corpses");

	// The party roster: corpse ids per owner, and where the server last placed a member.
	CorpseMemory::Memory roster;
	CorpseMemory::Owner member{70, "Ñandú", true, 5, true, {300, 400}, 0, {90}};
	roster.Track({member}, "cadáver de %s");
	Check(roster.Entities(5).empty(), "corpses already listed when tracking starts have no known place");
	member.corpses = {91, 90};
	roster.Track({member}, "cadáver de %s");
	const auto placed = roster.Entities(5);
	Check(placed.size() == 1 && placed[0].id == 91 && placed[0].position.x == 300 && placed[0].position.y == 400
		&& placed[0].name == "cadáver de Ñandú" && placed[0].category == EntityRadar::Category::Corpse,
		"a party member's new corpse is placed on another level where the dead member lies");
	CorpseMemory::Owner respawned = member, stranger{80, "Otro", false, 5, true, {10, 10}, 0, {93}};
	respawned.life = 100;
	respawned.corpses = {92, 91, 90};
	roster.Track({respawned, stranger}, "cadáver de %s");
	Check(roster.Entities(5).size() == 1, "a corpse first listed after respawn, or a stranger's, has no known place");
	roster.Observe(5, {Corpse(91, 310, 405), Corpse(95, 1, 1)}, Never);
	Check(roster.Entities(5).size() == 2 && roster.Entities(5)[0].position.x == 310,
		"reading the corpse itself corrects the roster's position");
	member.corpses = {92, 90};
	roster.Track({member}, "cadáver de %s");
	Check(roster.Entities(5).size() == 1 && roster.Entities(5)[0].id == 95,
		"a corpse that leaves the roster was taken back; one never listed is left to the reads");
	Passed();
}
