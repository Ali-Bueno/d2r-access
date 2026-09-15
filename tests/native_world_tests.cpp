#include "native_world.h"
#include "radar_config.h"
#include "test_support.h"

#include <array>
#include <cstring>

using namespace D2RAccess;
namespace {
// Minimal observed D2R structures. No process/game is required. The tests
// exercise pointer lifetime, dimensions, neighbor cycles and snapshot ownership.
template<size_t N> struct Memory {
	std::array<uint8_t, N> bytes{};
	uintptr_t Address() const { return reinterpret_cast<uintptr_t>(bytes.data()); }
	template<class T> void At(size_t offset, T value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); }
};
struct Fixture {
	Memory<0x200> unit, level;
	Memory<0x100> path, room, extended;
	Memory<0x28> grid;
	std::array<uint16_t, 100> cells{};
	std::array<uintptr_t, 2> neighbors{};
	Fixture() {
		unit.At<uint32_t>(8, 42); unit.At(0x38, path.Address());
		// Native collision getters read the high 16-bit words, not the projected
		// screen coordinates at +8/+C (values from the failed live camp session).
		path.At<uint16_t>(2, 104); path.At<uint16_t>(6, 104);
		path.At<int>(8, -2480); path.At<int>(12, 72816);
		path.At(0x20, room.Address()); path.At<int>(0x5C, 1);
		room.At(0x18, extended.Address()); room.At(0x38, grid.Address());
		extended.At(0x90, level.Address()); level.At<int>(0x1F8, 1);
		grid.At<int>(0, 100); grid.At<int>(4, 100); grid.At<int>(8, 10); grid.At<int>(12, 10);
		grid.At(0x20, reinterpret_cast<uintptr_t>(cells.data()));
	}
	bool Read(Radar::World& world) { return NativeWorld::ReadPlayer(unit.Address(), 42, 7, 1, world); }
};
}
int main() {
	Check(RadarConfig::Parse("[radar]\nenabled = false # quiet default\n").enabled == false, "embedded radar preference can disable startup audio");
	Check(RadarConfig::Parse("[other]\nenabled=false\n").enabled, "other features' switches cannot disable the radar");
	Check(!RadarConfig::Parse("[radar]\nenabled=FALSE\n").valid, "non-TOML boolean is rejected");
	Check(!RadarConfig::Parse("[radar]\nenabled=false\nenabled=true").valid, "duplicate radar setting is rejected");
	Check(RadarConfig::Parse("[radar]\r\n enabled = true\r\n").enabled, "Windows newlines and whitespace are accepted");
	Check(RadarConfig::Parse("").enabled, "missing preference uses the compiled default");
	Fixture fixture;
	Radar::World world;
	Check(fixture.Read(world), "verified-shaped live room snapshot reads successfully");
	Check(world.player.x == 104 && world.player.y == 104 && world.bodyPattern == 1, "position and actual footprint come from path getters' fields");
	fixture.path.At<uint16_t>(0, 0xFFFF); fixture.path.At<uint16_t>(4, 0x8000);
	Check(fixture.Read(world) && world.player.x == 104 && world.player.y == 104,
		"fractional position words and projected screen coordinates cannot corrupt collision coordinates");
	{
		Fixture distant;
		distant.path.At<uint16_t>(2, 40004); distant.path.At<uint16_t>(6, 50004);
		distant.grid.At<int>(0, 40000); distant.grid.At<int>(4, 50000);
		Radar::World distantWorld;
		Check(distant.Read(distantWorld) && distantWorld.player.x == 40004 && distantWorld.player.y == 50004,
			"collision coordinates use unsigned words as the native movzx getters do");
	}
	Check(world.session == 7 && world.level == 1 && world.rooms.size() == 1, "snapshot carries session and area identity");
	NativeWorld::Diagnostics detail;
	Check(NativeWorld::ReadPlayer(fixture.unit.Address(), 42, 7, 0, world, &detail) && world.level == 1,
		"ready player supplies the initial level before its lifecycle event arrives");
	Check(std::string_view(detail.stage) == "ready" && detail.playerId == 42 && detail.x == 104 && detail.level == 1,
		"successful native read reports the observed identity and position");
	fixture.level.At<int>(0x1F8, 0);
	Check(!NativeWorld::ReadPlayer(fixture.unit.Address(), 42, 7, 0, world, &detail) && std::string_view(detail.stage) == "level",
		"unknown expected level never accepts an invalid native level");
	fixture.level.At<int>(0x1F8, 1);
	Check(fixture.Read(world), "valid native level restores terrain after loading");
	fixture.cells[44] = 0x800;
	Check(world.Cell(104, 104) == Radar::State::Open, "snapshot owns its cells after game memory changes");
	Check(fixture.Read(world) && world.Cell(104, 104) == Radar::State::Blocked, "next snapshot sees dynamic door collision");
	fixture.cells[44] = 0;
	fixture.neighbors = {fixture.room.Address(), fixture.room.Address()};
	fixture.room.At(0, reinterpret_cast<uintptr_t>(fixture.neighbors.data()));
	fixture.room.At<uint32_t>(0x40, 2);
	Check(fixture.Read(world) && world.rooms.size() == 1, "self-neighbor and duplicate cycles terminate");
	fixture.unit.At<uint32_t>(8, 43);
	Check(!fixture.Read(world) && world.rooms.empty(), "wrong local player identity invalidates snapshot");
	fixture.unit.At<uint32_t>(8, 42);
	fixture.path.At<uintptr_t>(0x20, 1);
	Check(!fixture.Read(world), "inaccessible room pointer is contained");
	fixture.path.At(0x20, fixture.room.Address());
	fixture.level.At<int>(0x1F8, 2);
	Check(!fixture.Read(world), "room from a different area is rejected during transition");
	fixture.level.At<int>(0x1F8, 1);
	fixture.grid.At<int>(8, -1);
	Check(!fixture.Read(world), "negative dimensions cannot underflow allocation");
	fixture.grid.At<int>(8, 0x7fffffff);
	Check(!fixture.Read(world), "corrupt large dimensions are bounded");
	fixture.grid.At<int>(8, 10);
	fixture.grid.At<uintptr_t>(0x20, 1);
	Check(!fixture.Read(world), "inaccessible collision cells fail quiet");
	fixture.grid.At(0x20, reinterpret_cast<uintptr_t>(fixture.cells.data()));
	fixture.path.At<uint16_t>(2, 200);
	Check(!fixture.Read(world), "player outside copied collision bounds is rejected");
	fixture.path.At<uint16_t>(2, 104);
	fixture.path.At<int>(0x5C, 6);
	Check(!fixture.Read(world), "unknown body patterns are rejected instead of guessed");
	Check(!NativeWorld::ReadPlayer(fixture.unit.Address(), 42, 7, 1, world, &detail)
		&& std::string_view(detail.stage) == "footprint" && detail.pattern == 6,
		"failed footprint validation reports its actual stage and value");
	Passed();
}
