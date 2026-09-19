#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// D2R 3.3.93787 monster / world-object unit layout, recovered offline from the
// analyzed live image (unit field getters, the liveness test, the HUD nameplate
// code and the table accessors) and cross-checked byte-exact against the
// extracted CASC tables. Provenance, decompiled evidence and confidence per
// value: docs/entity-units-re.md. Shared unit/room offsets and the table bank
// resolver live in ground_item_layout.h.
namespace D2RAccess::EntityLayout {
// Unit+0 type values; 6 is stamped into freed units (0x349250) and returned for null.
inline constexpr uint32_t MonsterType = 1, ObjectType = 2, WarpType = 5;
inline constexpr size_t UnitFlags1 = 0x124, UnitFlags2 = 0x128;
inline constexpr uint32_t ForcedDead = 1u << 16; // Unit+0x124, liveness test 0x34C2C0
inline constexpr uint32_t PlayerMinion = 1u << 9; // Unit+0x124, getter 0x34FBA0 (inferred: set on player-owned spawns)
// Dynamic path (players/monsters): packed 16.16 X/Y at +0, room at +0x20 (native_world.cpp).
// Static path (items/objects): room at +0, integer X/Y at +0x10/+0x14 (ground_item_layout.h).
inline constexpr size_t PathRoom = 0x20;
// MonMode.txt / ObjMode.txt row indices.
inline constexpr uint32_t MonsterDeath = 0, MonsterDead = 12;
inline constexpr uint32_t ObjectModeCount = 8, ObjectNeutral = 0, ObjectOpened = 2;
// Table bank: row pointer / row count / row stride (accessors 0x976E0, 0x38FD00).
inline constexpr size_t MonStats = 0xF58, MonStatCount = 0xF60, MonStatStride = 0x1FC;
inline constexpr size_t Objects = 0x1528, ObjectCount = 0x1530, ObjectStride = 0x168;
// MonStats row: localized name id (0 = the game shows no name), alignment and flag bytes (monstats.txt columns).
inline constexpr size_t MonNameId = 0x36, MonAlign = 0x87, MonFlagsA = 0x3D;
inline constexpr uint8_t AlignHostile = 0, AlignFriendly = 1;
// monstats.txt NameStr shared by every row with no name of its own (42 rows:
// critters, spawners, the evil holes, town extras and the camp rogue scouts).
// The string table resolves this key to the game's placeholder label
// (local/lng/strings/ui.json id 5382, enUS "an evil force"), which the game
// itself never puts on a nameplate.
inline constexpr std::string_view PlaceholderNameKey = "dummy";
inline constexpr uint8_t FlagNpc = 1u << 0, FlagInteract = 1u << 1, FlagKillable = 1u << 7; // +0x3D bits
// MonsterData (*(Unit+0x10)): the game's own composed display name, cached by 0x985A0.
inline constexpr size_t MonCachedName = 0x40;
// Objects row: ASCII string key, per-mode Selectable bytes, door flag, SubClass byte and OpenWarp flag (objects.txt columns).
inline constexpr size_t ObjNameKey = 0x42, ObjSelectable = 0x85, ObjIsDoor = 0xFA, ObjSubClass = 0x127, ObjOpenWarp = 0x163;
// objects.txt OperateFn, the index of the game's interaction handler for the row.
inline constexpr size_t ObjOperateFn = 0x15E;
// The handlers whose rows are the containers a player opens for loot, read off
// the game's own objects.txt rather than guessed (tools/objects_containers.py,
// which also proves every offset above against its named column on all 587
// rows): 1 caskets and sarcophagi, 3 urns/jars/baskets, 4 chests, 5 and 7
// barrels, 14 crates/coffins/corpses/hollow logs, 30 the exploding chest,
// 39-41 and 57-59 the act-specific special chests, 68 the evil urn.
inline constexpr std::array<uint8_t, 14> ContainerOperateFns{1, 3, 4, 5, 7, 14, 30, 39, 40, 41, 57, 58, 59, 68};
// Together with "selectable while shut, not once open" these select 165 of the
// 587 rows (78 distinct keys). The count is the tripwire: if a game patch moves
// it, rerun the tool before trusting the cue.
inline constexpr size_t ContainerRowCount = 165;
// objects.txt SubClass bits the game's label builder (0xC17E0) and 0x34C470 branch on:
// shrine, town/permanent portal, arcane sanctuary portal / hell gate, waypoint.
inline constexpr uint8_t SubClassShrine = 1u << 0, SubClassPortal = 1u << 2, SubClassGate = 1u << 4, SubClassWaypoint = 1u << 6;
inline constexpr uint8_t SubClassExit = SubClassPortal | SubClassGate | SubClassWaypoint;
// ObjectData (*(Unit+0x10)): per-class parameter byte (the destination level id of a
// SubClass portal, 0x34AD40 / 0xC17E0); linked shrines.txt row and its localized name id.
inline constexpr size_t ObjPortalLevel = 8, ObjLinkRecord = 0x10, ShrineNameId = 0x12;
// Door captions by mode, string ids used by the door opener path (0x44FAC0).
inline constexpr uint32_t DoorClosedText = 0xC99, DoorOpenText = 0xC9B;
// Level exits are type-5 units in the same room list, positioned by the static path
// (0x2EFEE0, 0x324FC0). Unit+4 is the LvlWarp Id (compared as a byte, 0x3DA910); the
// room's RoomEx keeps a warp-node list whose node matches that Id (0x3DA9A0): the
// destination RoomEx, the next node, the gate the game requires before labelling the
// warp, and the LvlWarp row (stride identical to LvlWarp.bin: Id +0x2C, NoInteract +0x58).
inline constexpr size_t RoomExtended = 0x18, RoomWarpNodes = 0x78, ExtendedLevel = 0x90, LevelId = 0x1F8;
inline constexpr size_t WarpNodeDestination = 0, WarpNodeNext = 8, WarpNodeActive = 0x10, WarpNodeRow = 0x20;
inline constexpr size_t LvlWarpId = 0x2C, LvlWarpNoInteract = 0x58;
inline constexpr uint32_t WarpIdMask = 0xFF;
inline constexpr size_t MaxWarpNodes = 64;
// Levels table (accessor 0x32C4A0): in-memory rows are 8 bytes shorter than Levels.bin, so
// the LevelName key sits at +0xFD and the LevelWarp caption key ("To The ...") at +0x125
// (0xC17E0 reads the former, 0x9F2A0 the latter). Each key field is 40 bytes wide.
inline constexpr size_t Levels = 0x1460, LevelCount = 0x1468, LevelStride = 0x18C, LevelNameKey = 0xFD, LevelWarpKey = 0x125;
inline constexpr size_t MaxNameBytes = 128, MaxKeyBytes = 32, MaxLevelKeyBytes = LevelWarpKey - LevelNameKey;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
inline constexpr Evidence Checks[] = {
	{0x3614C0, 653, 0xA85E778CC89FCD70ULL}, // complete logical room list and tile rectangles
	{0x361040, 33, 0x884F085CB2600C15ULL}, // RoomEx+0x98 preset list
	{0x99650, 486, 0xBA686BCFA8BC8437ULL}, // preset linked-list traversal and native spawn coordinates
	{0x4926B0, 211, 0xF12E9167B6E9C9C5ULL}, // preset class/type and relative X/Y used for spawning
	{0x326480, 254, 0x3CEB6B5F0FFCE2D4ULL}, // room tile rectangle -> active subtile rectangle
	{0x334ED0, 17, 0xB98300209E9E7060ULL}, // five subtiles per tile
	{0x34B9D0, 45, 0x2D424E74A97DA816ULL}, // unit type getter (Unit+0), 6 for null
	{0x349860, 46, 0xA487037EF27A3BC8ULL}, // unit class getter (Unit+4)
	{0x34A330, 46, 0x0C4DF37279712714ULL}, // unit id getter (Unit+8)
	{0x34AB60, 43, 0xF767964C7BC0B888ULL}, // unit mode getter (Unit+0xC)
	{0x34A500, 53, 0xF1AC90AD0E70B541ULL}, // unit data getter (Unit+0x10)
	{0x34C2C0, 104, 0xE2BE7F578EF9FF9EULL}, // liveness: flags 0x124/0x128 and monster modes 0/12
	{0x34FBA0, 52, 0x0E05128D99586BAAULL}, // player-minion flag getter
	{0x34AD90, 81, 0xD3170B3ACE03F7DCULL}, // objects.txt record from ObjectData
	{0x985A0, 521, 0x50EC9372B676A70FULL}, // monster name builder, caches at MonsterData+0x40
	{0x976E0, 169, 0xFF5A28122B22038AULL}, // MonStats row accessor
	{0x38FD00, 132, 0x61F008DEA3273998ULL}, // Objects row accessor
	{0x324FC0, 384, 0x8B4A0736454E93DDULL}, // unit distance: static X/Y at +0x10/+0x14 for types 2, 4 and 5
	{0x2EFEE0, 161, 0xD55AB295152D9B73ULL}, // peer warp search: type-5 units in the room unit list, class = LvlWarp Id
	{0x3DA9A0, 168, 0x82E26127D267E177ULL}, // RoomEx+0x78 warp nodes: destination RoomEx, next, LvlWarp row (+0x2C Id)
	{0x3DA910, 88, 0xC5FFEDF233352A59ULL}, // warp unit -> its node, Id compared as a byte, gated on node+0x10
	{0x192B20, 5, 0x718E401FF86D81A2ULL}, // Room+0x18 -> RoomEx
	{0x361070, 137, 0x233046518CE86E42ULL}, // destination level id: RoomEx+0x90 -> Level+0x1F8
	{0x32C4A0, 246, 0x4AD6286EF6C1F499ULL}, // Levels row accessor (bank+0x1460 / +0x1468, stride 0x18C)
	{0x9F2A0, 203, 0xC90C7A453EC06F4BULL}, // warp caption: Levels row +0x125 key
	{0xC17E0, 910, 0x34C1A6F0CFE7D2B9ULL}, // object label: SubClass portal / waypoint bits, LevelName +0xFD, ObjectData+8
	{0x34AD40, 72, 0xAB8363B4E728C162ULL}, // ObjectData+8 byte getter
};
}
