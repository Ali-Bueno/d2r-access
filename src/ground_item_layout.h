#pragma once
#include <cstddef>
#include <cstdint>

// D2R 3.3.93787, recovered from the installed decrypted executable and its
// extracted ItemTypes.bin. Full provenance and fingerprint spans: docs/ground-items.md.
namespace D2RAccess::GroundItemLayout {
inline constexpr uintptr_t LocalIndex = 0x8B2D0, LocalPlayer = 0x9A480, TableBank = 0x300A90, GetStat = 0x2F5020;
inline constexpr size_t UnitType = 0, UnitClass = 4, UnitId = 8, UnitMode = 0xC,
	UnitData = 0x10, UnitPath = 0x38, UnitNext = 0x160, UnitBank = 0x1BD;
inline constexpr size_t PlayerRoom = 0x20, RoomNeighbors = 0, RoomNeighborCount = 0x40, RoomFirstUnit = 0xA8;
inline constexpr size_t StaticX = 0x10, StaticY = 0x14, ItemQuality = 0;
inline constexpr size_t Items = 0x15A0, ItemCount = 0x15A8, ItemStride = 0x1C0,
	ItemCode = 0x80, ItemName = 0xFC, ItemType = 0x12E, ItemQuest = 0x13A;
inline constexpr size_t Types = 0x1348, TypeCount = 0x1350, TypeStride = 0xE8;
// weapons.txt rangeadder (u8): the only offset matching all 306 rows of
// game-data/global/excel/weapons.bin, whose rows share this table's 0x1C0 layout.
inline constexpr size_t ItemRangeAdder = 0x115;
// ItemTypes.bin stores Code at +0 and two uint16 Equiv indices at +4/+6.
struct TypeRecord { uint32_t code; uint16_t parent1, parent2; };
inline constexpr uint32_t PlayerType = 0, ItemUnitType = 4;
// D2 item modes; native ground targeting/serialization retain ground and dropping.
inline constexpr uint32_t OnGround = 3, Dropping = 5;
inline constexpr int GoldStat = 14; // ItemStatCost.txt 'gold'; also native 0x370C90
inline constexpr int StashGoldStat = 15; // ItemStatCost.txt 'goldbank', the row after 'gold'
inline constexpr uint32_t Death = 0, Dead = 17; // native player modes
// D2C_PlayerModes (D2MOO Units/Player.h); native 0x7D96D0 tests run == 3 (docs/footsteps.md).
inline constexpr uint32_t Walk = 2, Run = 3, TownWalk = 6;
inline constexpr int StaminaStat = 10, MaxStaminaStat = 11; // ItemStatCost.txt 'stamina', 'maxstamina'
inline constexpr size_t MaxRooms = 64, MaxUnits = 4096, MaxTableRows = 65536, MaxTypeDepth = 16;
inline constexpr double PositionScale = 65536; // native unsigned 16.16 position
// `mayBeDetoured` marks a function the reader only ever calls, never reads for
// offsets: the loader is free to replace it with a jump to its own version.
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; bool mayBeDetoured = false; };
inline constexpr Evidence Checks[] = {
	{0x8B2D0, 7, 0x512D0A09BEF62F41ULL},
	{0x9A480, 165, 0x754EA91CD47DAFC1ULL},
	{0x2EFD90, 8, 0x92C8BD7E0C6E60FCULL},
	{0x34B4A0, 59, 0x8994082C41829018ULL},
	{0x2EFDE0, 13, 0x0746FEFCB398B135ULL},
	{0x34B440, 90, 0x5DC0EFDCF368FE6AULL},
	{0x266690, 249, 0xC4990DA6A749A376ULL},
	{0x36CF60, 113, 0xE1A2141ED682DC81ULL},
	{0x314110, 105, 0x6A64666C1906405DULL},
	{0x300A90, 67, 0xBC5C7D366A355A3BULL},
	{0x9A1B0, 651, 0x67A9431E258E9F3DULL},
	{0x293CC0, 2207, 0x605DCE341B5674CBULL},
	{0x34A0E0, 47, 0x05F93D9EFDC375A1ULL},
	{0x2F5020, 147, 0xEF955B08A436F777ULL, true}, // UNIT_GetStat: called, and detoured by 1.3.0-beta
};
}
