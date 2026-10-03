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
		path.At<uint32_t>(0x60, 0x80); // native player footprint flag
		path.At<uint32_t>(0x64, 0x1C09); // native movement-test mask
		room.At(0x18, extended.Address()); room.At(0x38, grid.Address());
		extended.At(0x90, level.Address()); level.At<int>(0x1F8, 1);
		grid.At<int>(0, 100); grid.At<int>(4, 100); grid.At<int>(8, 10); grid.At<int>(12, 10);
		grid.At(0x20, reinterpret_cast<uintptr_t>(cells.data()));
	}
	bool Read(Radar::World& world) { return NativeWorld::ReadPlayer(unit.Address(), 42, 7, 1, world); }
};
void PlayerPresenceTests() {
	Fixture fixture;
	Radar::World world;
	// The native pattern-1 setter stamps the actor's bit on a cross and adds
	// NO_PATH to its centre. A body probe overlaps that centre in every direction.
	for (size_t cell : {34u, 43u, 44u, 45u, 54u}) fixture.cells[cell] = 0x80;
	fixture.cells[44] |= 0x1000;
	Check(fixture.Read(world) && world.Body(105, 104) == Radar::State::Open,
		"the local player's own presence marker cannot block its body probe");
	Check(fixture.cells[44] == 0x1080, "removing self presence modifies only the owned snapshot");
	for (const auto [dx, dy] : std::array<std::pair<int, int>, 4>{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}})
		Check(world.Body(104 + 2 * dx, 104 + 2 * dy) == Radar::State::Open, "all body destinations can leave the occupied player cell");
	fixture.cells[44] |= 1;
	Check(fixture.Read(world) && world.Cell(104, 104) == Radar::State::Blocked, "a real wall sharing the player cell remains blocked");
	fixture.cells[44] = 0x1880;
	Check(fixture.Read(world) && world.Cell(104, 104) == Radar::State::Blocked, "a door bit sharing the player cell remains blocked");
	fixture.cells[44] = 0x1080;
	fixture.cells[45] = 0x1080;
	Check(fixture.Read(world) && world.Cell(105, 104) == Radar::State::Blocked, "no-path outside the local presence shape is preserved");
	fixture.cells[44] = 0x1000;
	Check(fixture.Read(world) && world.Cell(104, 104) == Radar::State::Blocked, "no-path without the local player's footprint bit is preserved");
	fixture.cells[44] = 0x1080;
	fixture.path.At<uint32_t>(0x60, 0);
	Check(fixture.Read(world) && world.Cell(104, 104) == Radar::State::Blocked, "zero footprint mask cannot erase terrain");
	fixture.path.At<uint32_t>(0x60, 0x80);
	fixture.path.At<int>(0x5C, 5);
	Check(fixture.Read(world) && world.Cell(104, 104) == Radar::State::Blocked, "a pattern without presence cannot erase no-path");
	fixture.path.At<int>(0x5C, 2);
	for (size_t cell : {34u, 43u, 44u, 45u, 54u, 55u}) fixture.cells[cell] = 0x1080;
	Check(fixture.Read(world) && world.Cell(104, 104) == Radar::State::Open && world.Cell(105, 104) == Radar::State::Open,
		"a large unit's native cross-shaped presence is removed");
	Check(world.Cell(105, 105) == Radar::State::Blocked, "large unit presence removal preserves no-path at square corners");
}
void MovementMaskTests() {
	Fixture fixture;
	Radar::World world;
	fixture.path.At<uint32_t>(0x64, 0x1D89); // also test actor presence
	fixture.cells[47] = 0x100;
	Check(fixture.Read(world) && world.walkingMask == 0x1D89 && world.Cell(107, 104) == Radar::State::Blocked,
		"radar uses the current native movement mask instead of a fixed skill-target mask");
	for (size_t cell : {34u, 43u, 44u, 45u, 54u}) fixture.cells[cell] = 0x80;
	fixture.cells[44] |= 0x1000;
	Check(fixture.Read(world) && world.Body(105, 104) == Radar::State::Open,
		"the player's full footprint is excluded when the movement mask includes actors");
	Check(fixture.cells[44] == 0x1080 && fixture.cells[45] == 0x80, "native occupancy remains unchanged");
	fixture.path.At<uint32_t>(0x64, 0x10000);
	Check(!fixture.Read(world), "out-of-range native collision mask fails closed");
}
void DisplacedPresenceTests() {
	Fixture fixture; Radar::World world;
	// Live camp 20:47:06: player (4463,4614), complete player cross at
	// (4464,4614). Down/right falsely hit NO_PATH; a down input crossed it.
	const auto stamp = [&](int x, int y) {
		for (const auto offset : std::array<int, 5>{0, -1, 1, -10, 10}) fixture.cells[y * 10 + x + offset] |= 0x80;
		fixture.cells[y * 10 + x] |= 0x1000;
	};
	stamp(5, 4);
	Check(fixture.Read(world) && world.Body(105, 105) == Radar::State::Open,
		"a complete displaced player stamp cannot masquerade as a blocked downward step");
	Check(world.Body(105, 103) == Radar::State::Open, "the same displaced stamp cannot block the rightward step");
	Check(fixture.cells[45] == 0x1080, "displaced presence removal never mutates game collision cells");
	for (const uint16_t solid : std::array<uint16_t, 4>{1, 8, 0x400, 0x800}) {
		fixture.cells[45] = 0x1080 | solid;
		Check(fixture.Read(world) && world.Cell(105, 104) == Radar::State::Blocked,
			"wall, door, object and no-player bits under a displaced stamp remain blocked");
	}
	fixture.cells[45] = 0x1080; fixture.cells[46] = 0;
	Check(fixture.Read(world) && world.Cell(105, 104) == Radar::State::Blocked,
		"an incomplete neighboring footprint cannot authorize clearing NO_PATH");
	fixture.cells.fill(0); stamp(5, 4); stamp(4, 5);
	Check(fixture.Read(world) && world.Cell(105, 104) == Radar::State::Blocked && world.Cell(104, 105) == Radar::State::Blocked,
		"two complete neighboring stamps are ambiguous and preserve both presence markers");
	fixture.cells.fill(0); fixture.cells[44] = 0x80; fixture.cells[45] = 0x1000;
	Check(fixture.Read(world) && world.Cell(105, 104) == Radar::State::Blocked,
		"an isolated neighboring NO_PATH bit is never treated as the player");
	fixture.cells.fill(0); stamp(5, 4); fixture.cells[35] |= 0x40;
	Check(fixture.Read(world) && world.Cell(105, 104) == Radar::State::Blocked,
		"water or missile overlap prevents displaced footprint inference");
	fixture.cells.fill(0); stamp(5, 4); fixture.cells[35] |= 0x100;
	Check(fixture.Read(world) && world.Cell(105, 104) == Radar::State::Blocked,
		"another actor overlapping the proposed footprint prevents its inference");
	fixture.cells.fill(0); stamp(5, 4); fixture.cells[35] |= 0x20;
	Check(fixture.Read(world) && world.Cell(105, 104) == Radar::State::Blocked,
		"unloaded footprint cells cannot authorize presence removal");
	for (const auto offset : std::array<std::array<int, 2>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
		fixture.cells.fill(0); stamp(4 + offset[0], 4 + offset[1]);
		NativeWorld::Diagnostics detail;
		Check(NativeWorld::ReadPlayer(fixture.unit.Address(), 42, 7, 1, world, &detail)
			&& detail.presenceOffsetX == offset[0] && detail.presenceOffsetY == offset[1]
			&& detail.rawPlayerCell == 0x80 && detail.removedPresenceCells == 1,
			"all four displaced crosses retain the original raw cell and report the selected offset");
	}
	// Whenever the reader may not shift its removal, the stamp keeps its own
	// footprint bit, so the terrain layer still reads a unit there instead of the
	// orphan NO_PATH barrier that produced the 2026-09-16 false wall cue.
	for (const uint16_t defeat : std::array<uint16_t, 3>{0x40, 0x100, 0x20}) {
		fixture.cells.fill(0); stamp(5, 4); fixture.cells[35] |= defeat;
		Check(fixture.Read(world) && world.Cell(105, 104) == Radar::State::Blocked
			&& world.Cell(105, 104, Radar::Layer::Terrain) == Radar::State::Open,
			"a displaced stamp that cannot be shifted keeps its footprint bit and never becomes terrain");
	}
	fixture.cells.fill(0); stamp(5, 4); stamp(4, 5);
	Check(fixture.Read(world) && world.Cell(105, 104, Radar::Layer::Terrain) == Radar::State::Open
		&& world.Cell(104, 105, Radar::Layer::Terrain) == Radar::State::Open,
		"two ambiguous stamps stay actors on the terrain layer instead of becoming two walls");
	fixture.cells.fill(0); fixture.path.At<int>(0x5C, 2);
	for (int y = 4; y <= 6; ++y) for (int x = 4; x <= 6; ++x)
		fixture.cells[y * 10 + x] = 0x80 | ((x == 5 || y == 5) ? 0x1000 : 0);
	Check(fixture.Read(world) && world.Cell(105, 105) == Radar::State::Open && world.Cell(106, 105) == Radar::State::Open,
		"a complete large footprint uses its native square and cross-shaped presence");
}
}
int main() {
	MovementMaskTests();
	PlayerPresenceTests();
	DisplacedPresenceTests();
	{
		using RadarConfig::ParseNavigation;
		Check(ParseNavigation("").valid && ParseNavigation("").fullAssistance,
			"navigation defaults to full assistance when omitted");
		Check(ParseNavigation("[navigation]\nfull_assistance = true\n").fullAssistance,
			"explicit true keeps full assistance");
		Check(ParseNavigation("[navigation]\nfull_assistance = false\n").valid
			&& !ParseNavigation("[navigation]\nfull_assistance = false\n").fullAssistance,
			"explicit false selects exploration-first routing");
		Check(!ParseNavigation("[navigation]\nfull_assistance = FALSE\n").valid
			&& ParseNavigation("[navigation]\nfull_assistance = FALSE\n").fullAssistance,
			"an invalid boolean falls back to full assistance");
		Check(!ParseNavigation("[navigation]\nfull_assistance\n").valid,
			"navigation setting needs a value");
		Check(!ParseNavigation("[navigation]\nfull_assistance = false\nfull_assistance = true\n").valid
			&& ParseNavigation("[navigation]\nfull_assistance = false\nfull_assistance = true\n").fullAssistance,
			"a duplicate navigation setting falls back to the default");
		Check(ParseNavigation("[radar]\nfull_assistance = false\n").fullAssistance,
			"navigation setting outside its section is ignored");
		Check(!ParseNavigation("[navigation]\nfull_assistance = false\n[radar]\nfull_assistance = true\n").fullAssistance,
			"another section cannot override navigation");
	}
	Check(RadarConfig::Parse("[radar]\nenabled = false # quiet default\n").enabled == false, "embedded radar preference can disable startup audio");
	Check(RadarConfig::Parse("[other]\nenabled=false\n").enabled, "other features' switches cannot disable the radar");
	Check(!RadarConfig::Parse("[radar]\nenabled=FALSE\n").valid, "non-TOML boolean is rejected");
	Check(!RadarConfig::Parse("[radar]\nenabled=false\nenabled=true").valid, "duplicate radar setting is rejected");
	Check(RadarConfig::Parse("[radar]\r\n enabled = true\r\n").enabled, "Windows newlines and whitespace are accepted");
	Check(!RadarConfig::Parse("").diagonals, "the rays follow the stick cardinals, the user's choice, unless configured otherwise");
	Check(RadarConfig::Parse("[radar]\ndiagonals = true\n").diagonals, "the stick diagonals, along the walls, can be chosen instead");
	Check(!RadarConfig::Parse("[radar]\ndiagonals = maybe\n").valid, "a non-boolean frame is rejected");
	Check(RadarConfig::Parse("").enabled, "missing preference uses the compiled default");
	Check(RadarConfig::Parse("[radar]\nvolume = 40 # quieter cues\n").volume == 40,
		"the documented volume percentage is read from the embedded config");
	Check(RadarConfig::Parse("").volume == RadarConfig::MaxVolume, "a missing volume keeps the compiled cue level");
	Check(RadarConfig::Parse("[radar]\nvolume=0\n").valid && RadarConfig::Parse("[radar]\nvolume=0\n").volume == 0,
		"zero percent is a valid silent preference, not an invalid value");
	Check(RadarConfig::Parse("[radar]\nvolume=100\n").volume == RadarConfig::MaxVolume,
		"the maximum documented percentage is accepted");
	Check(!RadarConfig::Parse("[radar]\nvolume=101\n").valid, "a percentage above the maximum is rejected");
	Check(!RadarConfig::Parse("[radar]\nvolume=-1\n").valid, "a negative percentage is rejected");
	Check(!RadarConfig::Parse("[radar]\nvolume=50.5\n").valid, "a fractional percentage is rejected");
	Check(!RadarConfig::Parse("[radar]\nvolume=loud\n").valid, "a non-numeric volume is rejected");
	Check(!RadarConfig::Parse("[radar]\nvolume=50\nvolume=60\n").valid, "duplicate radar volume is rejected");
	Check(RadarConfig::Parse("[radar]\nvolume=101\n").volume == RadarConfig::MaxVolume,
		"an invalid volume leaves the compiled default in place");
	Check(RadarConfig::Parse("[other]\nvolume=0\n").volume == RadarConfig::MaxVolume,
		"other features' volume cannot silence the radar");
	{
		const auto both = RadarConfig::Parse("[radar]\r\n enabled = false\r\n volume = 25 \r\n");
		Check(both.valid && !both.enabled && both.volume == 25, "both documented radar keys can be set together");
	}
	{
		const auto npc = RadarConfig::ParseNpc("[radar]\nenabled = false\n[npc]\r\n enabled = false # quiet\r\n range = 22.5\r\n line_of_sight = true\r\n");
		Check(npc.valid && npc.enabled == false && npc.range == 22.5 && npc.lineOfSight == true, "all documented [npc] keys parse in their own section");
		const auto absent = RadarConfig::ParseNpc("[radar]\nenabled = false\nrange = 5\n");
		Check(absent.valid && !absent.enabled && !absent.range && !absent.lineOfSight, "keys outside [npc] leave every announcer setting unset");
		Check(RadarConfig::ParseNpc("[npc]\nvolume = 3\n").valid, "unknown [npc] keys are ignored");
		Check(RadarConfig::ParseNpc("[npc]\nrange = 15\n").range == 15, "an integer range is accepted");
		Check(!RadarConfig::ParseNpc("[npc]\nrange = 0\n").valid && !RadarConfig::ParseNpc("[npc]\nrange = -2\n").valid, "a non-positive range is rejected");
		Check(!RadarConfig::ParseNpc("[npc]\nrange = far\n").valid && !RadarConfig::ParseNpc("[npc]\nrange\n").valid, "a non-numeric or valueless range is rejected");
		Check(!RadarConfig::ParseNpc("[npc]\nenabled = yes\n").valid && !RadarConfig::ParseNpc("[npc]\nline_of_sight = 1\n").valid, "non-TOML booleans are rejected");
		Check(!RadarConfig::ParseNpc("[npc]\nrange = 5\nrange = 6\n").valid && !RadarConfig::ParseNpc("[npc]\nenabled = true\nenabled = true\n").valid, "duplicate [npc] keys are rejected");
		Check(RadarConfig::Parse("[npc]\nenabled = false\n").enabled, "the announcer switch cannot disable the radar");
	}
	{
		const auto chests = RadarConfig::ParseChests("[radar]\nenabled = true\n[chests]\r\n enabled = false # too noisy\r\n");
		Check(chests.valid && chests.enabled == false, "the documented [chests] key parses in its own section");
		Check(RadarConfig::ParseChests("[radar]\nenabled = false\n").valid && !RadarConfig::ParseChests("[radar]\nenabled = false\n").enabled,
			"a key outside [chests] leaves the container loops unset");
		Check(RadarConfig::ParseChests("[chests]\nrange = 3\n").valid, "unknown [chests] keys are ignored");
		Check(!RadarConfig::ParseChests("[chests]\nenabled = yes\n").valid && !RadarConfig::ParseChests("[chests]\nenabled\n").valid,
			"a non-TOML boolean or a valueless key is rejected");
		Check(!RadarConfig::ParseChests("[chests]\nenabled = true\nenabled = false\n").valid, "a duplicate [chests] key is rejected");
		Check(RadarConfig::Parse("[chests]\nenabled = false\n").enabled, "the container switch cannot disable the radar");
		const auto doors = RadarConfig::ParseDoors("[chests]\nenabled = true\n[doors]\nenabled = false\n");
		Check(doors.valid && doors.enabled == false, "the documented [doors] key parses in its own section");
		Check(!RadarConfig::ParseDoors("[chests]\nenabled = false\n").enabled, "the container key leaves the door loops unset");
		Check(!RadarConfig::ParseDoors("[doors]\nenabled = 1\n").valid, "a non-TOML [doors] boolean is rejected");
	}
	Fixture fixture;
	Radar::World world;
	Check(fixture.Read(world), "verified-shaped live room snapshot reads successfully");
	Check(world.player.x == 104 && world.player.y == 104 && world.bodyPattern == 1, "position and actual footprint come from path getters' fields");
	fixture.path.At<uint16_t>(0, 0xFFFF); fixture.path.At<uint16_t>(4, 0x8000);
	Check(fixture.Read(world) && world.player.x == 104 && world.player.y == 104,
		"fractional position words and projected screen coordinates cannot corrupt collision coordinates");
	Check(world.subtileFraction.x == 65535.0 / 65536 && world.subtileFraction.y == 0.5,
		"native low words preserve exact motion within the collision cell");
	fixture.path.At<uint32_t>(0, (105u << 16) | 1u);
	Check(fixture.Read(world) && world.player.x == 105 && world.subtileFraction.x == 1.0 / 65536,
		"crossing a subtile boundary carries into the integer cell without losing motion precision");
	fixture.path.At<uint16_t>(2, 104);
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
	fixture.path.At<int>(0x5C, 0);
	Check(!NativeWorld::ReadPlayer(fixture.unit.Address(), 42, 7, 1, world, &detail)
		&& std::string_view(detail.stage) == "footprint" && detail.pattern == 0,
		"a point body is rejected for the local player, whose native size is hardcoded to the cross");
	{
		const auto cues = RadarConfig::ParseEntityCues("[npc]\nenabled = false\n[entity_cues]\nenabled = true\nnpcs = false\nenemies = true\nranged = false\nrange = 22.5\nvolume = 40\n");
		Check(cues.valid && cues.enabled == true && cues.npcs == false && cues.enemies == true
			&& cues.ranged == false && cues.range == 22.5 && cues.volume == 40u, "all entity-cue settings parse independently of NPC speech");
		const auto absent = RadarConfig::ParseEntityCues("[npc]\nenabled = false\nrange = 1\n");
		Check(absent.valid && !absent.enabled && !absent.range && !absent.volume && !absent.ranged,
			"absent entity section keeps compiled defaults");
		Check(RadarConfig::ParseEntityCues("[entity_cues]\nvolume = 0\n").volume == 0u, "entity cues accept explicit silence");
		for (const auto invalid : {"range = nan", "range = inf", "range = 0", "range = -1", "range = 101",
			"volume = -1", "volume = 101", "volume = 1.5", "enabled = yes", "npcs = 1", "ranged", "enemies = false\nenemies = true",
			"volume = 0\nvolume = 1", "range = 15\nrange = 15"}) {
			const auto parsed = RadarConfig::ParseEntityCues(std::string("[entity_cues]\n") + invalid + "\n");
			Check(!parsed.valid && !parsed.enabled && !parsed.range && !parsed.volume, "invalid entity settings discard the entire section");
		}
	}
	Passed();
}
