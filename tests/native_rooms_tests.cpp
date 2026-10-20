#include "native_rooms.h"
#include "entity_layout.h"
#include "level_exit_layout.h"
#include "test_support.h"
#include <array>
#include <cstring>
#include <limits>

using namespace D2RAccess;
using namespace LevelExitLayout;
using namespace EntityLayout;
namespace {
template<size_t N> struct Memory {
	std::array<uint8_t, N> data{};
	uintptr_t Address() const { return reinterpret_cast<uintptr_t>(data.data()); }
	template<class T> void At(size_t offset, T value) { std::memcpy(data.data() + offset, &value, sizeof(value)); }
};
// Three rooms linked in an order that is deliberately NOT the sorted order, so
// a test that passes proves the reader sorts rather than inheriting the game's
// generation order.
struct Fixture {
	Memory<0xC0> active, south, north, east;
	Memory<0x200> level, otherLevel;
	Fixture() {
		active.At(RoomExtended, north.Address());
		for (auto* room : {&north, &south, &east}) {
			room->At(ExtendedLevel, level.Address());
			room->At<int>(RoomTileWidth, 8); room->At<int>(RoomTileHeight, 8);
		}
		level.At<uint32_t>(LevelId, 9);
		level.At(LevelFirstRoom, south.Address());
		south.At(RoomNext, east.Address());
		east.At(RoomNext, north.Address());
		north.At<int>(RoomTileX, 20); north.At<int>(RoomTileY, 10);
		south.At<int>(RoomTileX, 20); south.At<int>(RoomTileY, 30);
		east.At<int>(RoomTileX, 40);  east.At<int>(RoomTileY, 10);
	}
	bool Read(std::vector<NativeRooms::Room>& rooms) { return NativeRooms::Read(active.Address(), rooms); }
};
}

int main() {
	Fixture f;
	std::vector<NativeRooms::Room> rooms;
	Check(f.Read(rooms) && rooms.size() == 3, "every logical room of the level is read, active or not");
	Check(rooms[0].tileY == 10 && rooms[0].tileX == 20, "rooms sort by tile row then column, not by native link order");
	Check(rooms[1].tileX == 40 && rooms[2].tileY == 30, "the remaining rooms follow in the same stable order");

	const auto bounds = NativeRooms::Subtiles(rooms[0]);
	Check(bounds.minX == 100 && bounds.minY == 50, "tile origins convert to subtiles at five subtiles per tile");
	Check(bounds.maxX == 140 && bounds.maxY == 90, "the subtile rectangle is half-open, so adjacent rooms never overlap");
	Check(NativeRooms::Contains(bounds, 100, 50) && !NativeRooms::Contains(bounds, 140, 50), "containment excludes the far edge");

	Check(NativeRooms::Containing(rooms, {105, 55}) == 0, "a position inside a room finds it");
	Check(NativeRooms::Containing(rooms, {105, 200}) == NativeRooms::None, "a position in no room reports none rather than the nearest");
	Check(NativeRooms::Containing(rooms, {std::numeric_limits<double>::quiet_NaN(), 0}) == NativeRooms::None, "nonfinite positions cannot select a room");

	Check(NativeRooms::Identity(20, 10) != 0 && NativeRooms::Identity(20, 10) != NativeRooms::Identity(10, 20), "identity distinguishes rooms by rectangle origin");
	Check(NativeRooms::Identity(0, 0) != 0, "the origin does not collide with the no-room sentinel");
	Check(NativeRooms::Identity(-1, 0) == 0 && NativeRooms::Identity(65536, 0) == 0, "out-of-range origins are rejected");
	Check(rooms[0].id == NativeRooms::Identity(20, 10), "a read room carries the same identity the helper computes");

	f.east.At(ExtendedLevel, f.otherLevel.Address());
	Check(!f.Read(rooms) && rooms.empty(), "a foreign level pointer fails closed with no partial list");
	f.east.At(ExtendedLevel, f.level.Address());
	f.east.At(RoomNext, f.south.Address());
	Check(!f.Read(rooms) && rooms.empty(), "cyclic room links are bounded and yield nothing");
	f.east.At(RoomNext, f.north.Address());
	f.south.At<int>(RoomTileWidth, std::numeric_limits<int>::max());
	Check(!f.Read(rooms) && rooms.empty(), "corrupt dimensions cannot overflow the subtile conversion");
	f.south.At<int>(RoomTileWidth, 8);
	f.north.At<int>(RoomTileHeight, 0);
	Check(!f.Read(rooms) && rooms.empty(), "a degenerate rectangle is refused rather than routed through");
	f.north.At<int>(RoomTileHeight, 8);
	Check(f.Read(rooms) && rooms.size() == 3, "the fixture restores");
	Check(NativeRooms::Read(f.active.Address(), rooms, 9) && rooms.size() == 3, "the expected level reads");
	Check(!NativeRooms::Read(f.active.Address(), rooms, 8) && rooms.empty(), "another level's rooms are refused, with no stale list");
	Check(!NativeRooms::Read(0, rooms) && rooms.empty(), "no player room clears any previous list");
	Passed();
}
