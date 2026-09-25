#include "native_level_areas.h"
#include "level_areas.h"
#include "drlg_layout.h"
#include "entity_layout.h"
#include "level_exit_layout.h"
#include "test_support.h"
#include <array>
#include <cstring>
#include <limits>

using namespace D2RAccess;
using namespace LevelExitLayout;
using namespace EntityLayout;
using DrlgLayout::DrlgFirstLevel, DrlgLayout::LevelDrlg, DrlgLayout::LevelNext;
namespace {
template<size_t N> struct Memory {
	std::array<uint8_t, N> data{};
	uintptr_t Address() const { return reinterpret_cast<uintptr_t>(data.data()); }
	template<class T> void At(size_t offset, T value) { std::memcpy(data.data() + offset, &value, sizeof(value)); }
};
// The act as the game keeps it: a DRLG holding a list of the levels it has
// built, each with its own list of rooms. The town sits beside a wilderness
// twice its size, and a cave is placed at its own far-off offset.
struct Fixture {
	Memory<0xC0>  active, townRoom, moorRoomA, moorRoomB, caveRoom;
	Memory<0x200> town, moor, cave;
	Memory<0x900> drlg;
	Fixture() {
		active.At(RoomExtended, townRoom.Address());
		townRoom.At(ExtendedLevel, town.Address());
		moorRoomA.At(ExtendedLevel, moor.Address()); moorRoomB.At(ExtendedLevel, moor.Address());
		caveRoom.At(ExtendedLevel, cave.Address());
		for (auto* pair : {&town, &moor, &cave}) pair->At(LevelDrlg, drlg.Address());
		drlg.At(DrlgFirstLevel, town.Address());
		town.At<uint32_t>(LevelId, 1); moor.At<uint32_t>(LevelId, 2); cave.At<uint32_t>(LevelId, 8);
		town.At(LevelNext, moor.Address()); moor.At(LevelNext, cave.Address());
		town.At(LevelFirstRoom, townRoom.Address());
		moor.At(LevelFirstRoom, moorRoomA.Address()); moorRoomA.At(RoomNext, moorRoomB.Address());
		cave.At(LevelFirstRoom, caveRoom.Address());
		// Town 56x40 at 1100,1080; the moor 80x80 directly to its right, in
		// two rooms; the cave at the act's own offset, far away.
		townRoom.At<int>(RoomTileX, 1100); townRoom.At<int>(RoomTileY, 1080);
		townRoom.At<int>(RoomTileWidth, 56); townRoom.At<int>(RoomTileHeight, 40);
		moorRoomA.At<int>(RoomTileX, 1156); moorRoomA.At<int>(RoomTileY, 1080);
		moorRoomA.At<int>(RoomTileWidth, 40); moorRoomA.At<int>(RoomTileHeight, 80);
		moorRoomB.At<int>(RoomTileX, 1196); moorRoomB.At<int>(RoomTileY, 1080);
		moorRoomB.At<int>(RoomTileWidth, 40); moorRoomB.At<int>(RoomTileHeight, 80);
		caveRoom.At<int>(RoomTileX, 1500); caveRoom.At<int>(RoomTileY, 1000);
		caveRoom.At<int>(RoomTileWidth, 60); caveRoom.At<int>(RoomTileHeight, 60);
	}
	bool Read(std::vector<NativeLevelAreas::Area>& areas) { return NativeLevelAreas::Read(active.Address(), areas); }
};
}

int main() {
	Fixture f;
	std::vector<NativeLevelAreas::Area> areas;
	Check(f.Read(areas) && areas.size() == 3, "every level the act has built is reported");
	Check(areas[0].level == 1 && areas[0].tileX == 1100 && areas[0].tileWidth == 56 && areas[0].rooms == 1,
		"a level's footprint is the rectangle its rooms occupy");
	Check(areas[1].level == 2 && areas[1].tileX == 1156 && areas[1].tileWidth == 80 && areas[1].rooms == 2,
		"several rooms are unioned into one footprint");

	auto near = LevelAreas::Neighbours(areas, 1);
	Check(near.size() == 1 && near[0].level == 2, "only the area that touches this one is a way out of it");
	Check(near[0].gapTiles == 0, "abutting areas have no gap");
	// The shared edge runs 1080..1120 on Y; the crossing is its middle, on the
	// last subtile of this area, so it stands in one of this level's rooms.
	Check(near[0].position.x == 1156 * SubtilesPerTile - 1, "a neighbour to the east is crossed at this area's last subtile");
	Check(near[0].position.y == 1100 * SubtilesPerTile, "and in the middle of the span the two areas share");
	const auto back = LevelAreas::Neighbours(areas, 2);
	Check(back.size() == 1 && back[0].position.x == 1156 * SubtilesPerTile,
		"a neighbour to the west is crossed at this area's first subtile");
	Check(LevelAreas::Neighbours(areas, 8).empty(), "an area the act placed at its own offset reaches nothing");
	Check(LevelAreas::Neighbours(areas, 99).empty(), "a level that is not in the list has no neighbours");

	// The crossing is a property of the two rectangles, so it does not move as
	// the player does -- the landmark stays put while they walk toward it.
	const auto again = LevelAreas::Neighbours(areas, 1);
	Check(again[0].position.x == near[0].position.x && again[0].position.y == near[0].position.y,
		"the same two areas always meet in the same place");

	// Six tiles is the game's own "near": at five the areas are still a way
	// out of each other, at six they are not.
	f.moorRoomA.At<int>(RoomTileX, 1161); f.moorRoomB.At<int>(RoomTileX, 1201);
	Check(f.Read(areas) && LevelAreas::Neighbours(areas, 1).size() == 1, "a gap under six tiles is still a crossing");
	Check(LevelAreas::Neighbours(areas, 1)[0].gapTiles == 5, "and the gap is reported");
	f.moorRoomA.At<int>(RoomTileX, 1162); f.moorRoomB.At<int>(RoomTileX, 1202);
	Check(f.Read(areas) && LevelAreas::Neighbours(areas, 1).empty(), "six tiles apart is not somewhere you can walk to");
	f.moorRoomA.At<int>(RoomTileX, 1156); f.moorRoomB.At<int>(RoomTileX, 1196);

	// Fail closed: nothing partial, and nothing from another act.
	f.moor.At(LevelDrlg, f.cave.Address());
	Check(!f.Read(areas) && areas.empty(), "a level that does not belong to this DRLG stops the walk");
	f.moor.At(LevelDrlg, f.drlg.Address());
	f.cave.At(LevelNext, f.town.Address());
	Check(!f.Read(areas) && areas.empty(), "a cyclic level list is bounded and reports nothing");
	f.cave.At<uintptr_t>(LevelNext, 0);
	f.moorRoomB.At(RoomNext, f.moorRoomA.Address());
	Check(!f.Read(areas) && areas.empty(), "a cyclic room list is bounded too");
	f.moorRoomB.At<uintptr_t>(RoomNext, 0);
	f.townRoom.At<int>(RoomTileWidth, std::numeric_limits<int>::max());
	Check(!f.Read(areas), "corrupt dimensions cannot overflow the footprint");
	f.townRoom.At<int>(RoomTileWidth, 56);
	Check(f.Read(areas) && areas.size() == 3, "the fixture restores");
	f.cave.At<uintptr_t>(LevelFirstRoom, 0);
	Check(f.Read(areas) && areas.size() == 2, "a level with no rooms yet has no place to point at");
	Check(!NativeLevelAreas::Read(0, areas) && areas.empty(), "no player room clears previous areas");
	Passed();
}
