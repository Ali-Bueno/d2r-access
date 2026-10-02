#pragma once
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
// D2Client UI 0xCE8F0 uses mode 17 for a player corpse. D2Game corpse pickup
// 0x4B2C8A checks a type-0 unit's inventory link ID against the player ID.
inline constexpr uint32_t PlayerCorpseMode = 17;
// PlrMode.txt DT; liveness 0x34C2C0 treats a type-0 unit in mode 0 or 17 as dead.
inline constexpr uint32_t PlayerDeathMode = 0;
// PlayerData (*(Unit+0x10), getter 0x34B240): the character name, a NUL-terminated
// char[0x3D] at +0 (copy bound in 0x1C9DE0); the quest array follows at +0x40 (0x5A6650).
inline constexpr size_t PlayerName = 0, PlayerNameBytes = 0x3D;
inline constexpr std::string_view PlayerCorpseNameKey = "DeadPlayerCorpseName";
// Unit inventory getter 0x34A360: +0x90 normally, or +0x98 while the +0xA0
// owner reference is live. Inventory link-ID getter 0x388BA0 validates magic.
inline constexpr size_t UnitInventory = 0x90, UnitOwnedInventory = 0x98, UnitInventoryRef = 0xA0;
inline constexpr size_t InventoryRefCount = 8, InventoryLinkId = 0x48;
inline constexpr uint32_t InventoryMagic = 0x01020304;
// An item's socketed gems and runes are its own inventory's items. Owner unit at
// +8 (0x3856E0), item count at +0x4C (0x3856E0 increments it, 0x38BA30 decrements).
// Live 2026-09-28: a sword with one filled socket read 1. docs/inventory-npc-followup.md#socket-contents
inline constexpr size_t InventoryOwner = 8, InventoryItemCount = 0x4C;
inline constexpr uint32_t MaxSockets = 6; // ItemsTxt gemsockets tops out at 6
inline constexpr size_t UnitFlags1 = 0x124, UnitFlags2 = 0x128;
inline constexpr uint32_t ForcedDead = 1u << 16; // Unit+0x124, liveness test 0x34C2C0
inline constexpr uint32_t PlayerMinion = 1u << 9; // Unit+0x124, getter 0x34FBA0 (inferred: set on player-owned spawns)
// Dynamic path (players/monsters): packed 16.16 X/Y at +0, room at +0x20 (native_world.cpp).
// Static path (items/objects): room at +0, integer X/Y at +0x10/+0x14 (ground_item_layout.h).
inline constexpr size_t PathRoom = 0x20;
// MonMode.txt / ObjMode.txt row indices.
inline constexpr uint32_t MonsterDeath = 0, MonsterDead = 12;
inline constexpr uint32_t ObjectModeCount = 8, ObjectNeutral = 0, ObjectOperating = 1, ObjectOpened = 2;
// Table bank: row pointer / row count / row stride (accessors 0x976E0, 0x38FD00).
inline constexpr size_t MonStats = 0xF58, MonStatCount = 0xF60, MonStatStride = 0x1FC;
inline constexpr size_t Objects = 0x1528, ObjectCount = 0x1530, ObjectStride = 0x168;
// MonStats row: localized name id (0 = the game shows no name), alignment and flag bytes (monstats.txt columns).
inline constexpr size_t MonNameId = 0x36, MonAlign = 0x87, MonFlagsA = 0x3D, MonFlagsBoss = 0x3C, MonFlagsRanged = 0x3F;
inline constexpr uint8_t FlagBoss = 1u << 6, FlagPrimeEvil = 1u << 7, FlagRanged = 1u << 4;
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
inline constexpr size_t MonRarityFlags = 0x1A;
inline constexpr uint16_t MonEliteMask = 0x000F; // native 0x38E0E0(unit, 0) special-monster test
// Superunique (Rakanishu, Bishibosh...): client setup 0x98FA0 stores the
// SuperUniques row (+0x2A, via 0x38E810) only when this bit is set.
inline constexpr uint16_t MonSuperUnique = 0x0002;
// Objects row: ASCII string key, per-mode Selectable bytes, door flag, SubClass byte and OpenWarp flag (objects.txt columns).
inline constexpr size_t ObjNameKey = 0x42, ObjSelectable = 0x85, ObjIsDoor = 0xFA, ObjSubClass = 0x127, ObjOpenWarp = 0x163;
// Size getters 0x3481E0/0x3482C0; all columns cross-checked by tools/objects_containers.py.
inline constexpr size_t ObjSizeX = 0x90, ObjSizeY = 0x94, ObjHasCollision = 0xE0;
// objects.txt OperateFn, the index of the game's interaction handler for the row.
inline constexpr size_t ObjOperateFn = 0x15E;
inline constexpr uint8_t CairnStoneOperateFn = 9; // objects.txt StoneAlpha through StoneLambda
// The handlers whose rows are the containers a player opens for loot, read off
// the game's own objects.txt rather than guessed (tools/objects_containers.py,
// which also proves every offset above against its named column on all 587
// rows): 1 caskets and sarcophagi, 3 urns/jars/baskets, 4 chests, 5 and 7
// barrels, 14 crates/coffins/corpses/hollow logs, 30 the exploding chest,
// 39-41 and 57-59 the act-specific special chests, 68 the evil urn.
inline constexpr std::array<uint8_t, 14> ContainerOperateFns{1, 3, 4, 5, 7, 14, 30, 39, 40, 41, 57, 58, 59, 68};
// Together with "selectable while shut, not once open" these select 165 of the
// 587 rows (78 distinct keys). Both counts are the tripwire `access-items
// chests-dump` checks against the live table: if a patch moves either, rerun
// the tool before trusting the cue.
inline constexpr size_t ContainerRowCount = 165, ObjectRowCount = 587;
// objects.txt SubClass bits the game's label builder (0xC17E0) and 0x34C470 branch on:
// shrine, town/permanent portal, arcane sanctuary portal / hell gate, waypoint.
// Bit meanings derived from the game's own global/excel/objects.txt (D2R 3.3,
// 588 rows): bit0 is carried by 92 Shrine rows, bit1 by the two Obelisks, bit2
// by the two Portals, bit3 by 132 containers (Casket, Urn, chest), bit4 by the
// two lowercase "portal" gates, bit5 by the five Wells and bit6 by 16
// Waypoints. Not guessed from the classic objects.txt documentation.
inline constexpr uint8_t SubClassShrine = 1u << 0, SubClassObelisk = 1u << 1, SubClassPortal = 1u << 2,
	SubClassContainer = 1u << 3, SubClassGate = 1u << 4, SubClassWell = 1u << 5, SubClassWaypoint = 1u << 6;
inline constexpr uint8_t SubClassExit = SubClassPortal | SubClassGate | SubClassWaypoint;
// ObjectData (*(Unit+0x10)): per-class parameter byte (the destination level id of a
// SubClass portal, 0x34AD40 / 0xC17E0); linked shrines.txt row and its localized name id.
inline constexpr size_t ObjPortalLevel = 8, ObjLinkRecord = 0x10, ShrineNameId = 0x12;
// Door captions by mode, string ids used by the door opener path (0x44FAC0).
inline constexpr uint32_t DoorClosedText = 0xC99, DoorOpenText = 0xC9B;
// The player's own stash carries no SubClass bit, so it is recognised by the
// name key in its objects.txt row. Confirmed against the game's string table:
// local/lng/strings/objects.json maps "bank" to "Your Private Stash".
inline constexpr const char* BankNameKey = "bank";
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
	{0x3481E0, 212, 0x972CCB30B7F79BBFULL}, // object SizeX: row+0x90
	{0x3482C0, 244, 0x56884EA7EFC4EB3BULL}, // object SizeY: row+0x94
	{0x34BBE0, 30, 0x5C3897F892CA0ECEULL}, // HasCollision[mode]: row+0xE0
	{0x349890, 187, 0xDA8838ED9D4DA150ULL}, // ordinary object collision mask 0x400
	{0x363C80, 99, 0xBE3C2A1BD0E88572ULL}, // object rectangle: centre - size/2, inclusive size
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
inline constexpr Evidence PlayerChecks[] = {
	{0x34B240, 68, 0xB067440C053D4F5CULL}, // player-data getter: type 0 asserted, returns Unit+0x10
	{0x134FC0, 265, 0x91522CA1BE41A651ULL}, // Roster.cpp name getter: PlayerData itself is the name string
	{0x1C9DE0, 61, 0xE8F22C8774858297ULL}, // name copy bounded by 0x3D bytes at PlayerData+0
	{0x5A6650, 83, 0xD8D67EDFC6B8BD61ULL}, // quest array at PlayerData+0x40 ends the name buffer
};
// Client party roster (D2Client Roster.cpp): one entry per player, the local one first.
// RosterNext 0x136470 returns the head global (load at 0x13647D) for a null entry.
inline constexpr uintptr_t RosterHead = 0x2A4DB90;
// Entry: name char[0x3D] (init 0x13A220), unit id (0x1366E0), party id word (0x136570),
// level id (0x136440, packet 0x7F), subtile X/Y (0x136450, packet 0x90), next (0x136470).
inline constexpr size_t RosterName = 0, RosterUnitId = 0x48, RosterParty = 0x5A, RosterLevel = 0x5C,
	RosterX = 0x60, RosterY = 0x64, RosterNext = 0x148;
inline constexpr uint16_t NoParty = 0xFFFF; // 0x136570's "not in a party"
inline constexpr size_t MaxRosterEntries = 64; // cycle guard, not a game limit
inline constexpr Evidence RosterChecks[] = {
	{0x136470, 21, 0x4F66E997F9BC6D8EULL}, // next entry; head global for null
	{0x13A220, 332, 0x6D4A070900DAFDDAULL}, // entry init: name at +0 bounded by 0x3D, party id +0x5A, unit id +0x48
	{0x1366E0, 9, 0x9BB24217A6F40590ULL}, // unit id +0x48
	{0x136570, 70, 0x2AA4002801742ACCULL}, // party id +0x5A, 0xFFFF unless shared
	{0x136440, 9, 0x9BF63A17A72DC4C4ULL}, // level id +0x5C
	{0x136450, 17, 0x7C306C78E1D0987BULL}, // X/Y +0x60/+0x64
	{0x12DE30, 15, 0xFC497D047C5051D3ULL}, // packet 0x90 handler: unit id, X, Y
	{0x139420, 63, 0x9562FD5F4034BD6BULL}, // stores the 0x90 position at +0x60/+0x64
	{0x480AD0, 461, 0x4E34945C8B6B454EULL}, // server: unit path X/Y, sent beyond 50 subtiles
	{0xD286C, 561, 0x3BE9A00FE07BA259ULL}, // automap marker: same party, unit not loaded, same layer
};
// Roster corpses (docs/player-corpse-re.md): packet 0x8E assign prepends a node
// {+0 corpse id, +8 next} to the owner's list at +0x78, unassign removes it on
// pickup. Life percent at +0x4C, stored from packet 0x7F.
inline constexpr size_t RosterLife = 0x4C, RosterCorpses = 0x78, CorpseNodeId = 0, CorpseNodeNext = 8;
inline constexpr size_t MaxRosterCorpses = 16; // server cap: 0x42C5D7 skips creating a 16th
inline constexpr Evidence RosterCorpseChecks[] = {
	{0x12DE10, 29, 0x9C1AF17D2B65342BULL}, // packet 0x7F: party flag, life word +2
	{0x139460, 1066, 0x8E9FE84BC7E56B4CULL}, // stores life +0x4C and level +0x5C
	{0x1364A0, 144, 0xE34E738B8459B339ULL}, // life getter +0x4C
	{0x12E1B0, 23, 0x417C2D277B098B63ULL}, // packet 0x8E: assign or unassign
	{0x136800, 305, 0x84D2F1F985B870FCULL}, // assign: corpse id node on the owner's +0x78
	{0x138150, 198, 0x1ADC6CE0B4205817ULL}, // unassign
};
inline constexpr Evidence SocketChecks[] = {
	{0x34A360, 165, 0xCC327F6CEB48B813ULL}, // inventory pointer, including owner wrapper
	{0x388BA0, 100, 0xB5FBC7641E1D7F3AULL}, // inventory magic and link ID
	{0x388C10, 68, 0xCF3B0CFFC3095301ULL}, // first item: magic check, then Inv+0x10
	{0x3856E0, 378, 0x969ACF37B5669D2AULL}, // socket insert: owner Inv+8, count Inv+0x4C
};
inline constexpr Evidence CorpseChecks[] = {
	{0x34A360, 165, 0xCC327F6CEB48B813ULL}, // inventory pointer, including owner wrapper
	{0x388BA0, 100, 0xB5FBC7641E1D7F3AULL}, // inventory magic and link ID
	{0xCE8F0, 344, 0xFBF8C68D0D6D122AULL}, // corpse mode and displayed name
	{0x4B2C8A, 1679, 0x5F479D4462F948A0ULL}, // pickup compares link ID with player ID
	{0x12D7E0, 66, 0x8C76B7E4ADFCAF4FULL}, // packet 0x74: owner and corpse ids
	{0xFF910, 431, 0x00E34CF334B2272AULL}, // 0x74 sets the corpse inventory's link ID
};
}
