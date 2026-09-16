#pragma once
#include <cstddef>
#include <cstdint>

// D2R 3.3.93787 monster / world-object unit layout, recovered offline from the
// analyzed live image (unit field getters, the liveness test, the HUD nameplate
// code and the table accessors) and cross-checked byte-exact against the
// extracted CASC tables. Provenance, decompiled evidence and confidence per
// value: docs/entity-units-re.md. Shared unit/room offsets and the table bank
// resolver live in ground_item_layout.h.
namespace D2RAccess::EntityLayout {
// Unit+0 type values; 6 is stamped into freed units (0x349250) and returned for null.
inline constexpr uint32_t MonsterType = 1, ObjectType = 2;
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
inline constexpr uint8_t FlagNpc = 1u << 0, FlagInteract = 1u << 1, FlagKillable = 1u << 7; // +0x3D bits
// MonsterData (*(Unit+0x10)): the game's own composed display name, cached by 0x985A0.
inline constexpr size_t MonCachedName = 0x40;
// Objects row: ASCII string key, per-mode Selectable bytes, door and shrine flags (objects.txt columns).
inline constexpr size_t ObjNameKey = 0x42, ObjSelectable = 0x85, ObjIsDoor = 0xFA, ObjIsShrine = 0x127;
// ObjectData (*(Unit+0x10)): linked shrines.txt row; its localized name id.
inline constexpr size_t ObjLinkRecord = 0x10, ShrineNameId = 0x12;
// Door captions by mode, string ids used by the door opener path (0x44FAC0).
inline constexpr uint32_t DoorClosedText = 0xC99, DoorOpenText = 0xC9B;
inline constexpr size_t MaxNameBytes = 128, MaxKeyBytes = 32;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
inline constexpr Evidence Checks[] = {
	{0x34B9D0, 45, 0xA892FC2D16626027ULL}, // unit type getter (Unit+0), 6 for null
	{0x349860, 46, 0x27DC60F1541D2802ULL}, // unit class getter (Unit+4)
	{0x34A330, 46, 0x7626C6CCA52BFF23ULL}, // unit id getter (Unit+8)
	{0x34AB60, 43, 0xBCDE6EC19007BD7AULL}, // unit mode getter (Unit+0xC)
	{0x34A500, 53, 0x8D3BD69276CB0C27ULL}, // unit data getter (Unit+0x10)
	{0x34C2C0, 104, 0xBF5FE9280CD1598FULL}, // liveness: flags 0x124/0x128 and monster modes 0/12
	{0x34FBA0, 52, 0x7BAC2EF8F8795A3DULL}, // player-minion flag getter
	{0x34AD90, 81, 0x2BADCEB575F7FFEFULL}, // objects.txt record from ObjectData
	{0x985A0, 521, 0x69116D401817738CULL}, // monster name builder, caches at MonsterData+0x40
	{0x976E0, 169, 0x262A2516AE7AEA21ULL}, // MonStats row accessor
	{0x38FD00, 132, 0x8E99DD2AE136F14FULL}, // Objects row accessor
};
}
