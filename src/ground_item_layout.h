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
	ItemCode = 0x80, ItemName = 0xFC, ItemType = 0x12E;
inline constexpr size_t Types = 0x1348, TypeCount = 0x1350, TypeStride = 0xE8;
// ItemTypes.bin stores Code at +0 and two uint16 Equiv indices at +4/+6.
struct TypeRecord { uint32_t code; uint16_t parent1, parent2; };
inline constexpr uint32_t PlayerType = 0, ItemUnitType = 4;
// D2 item modes; native ground targeting/serialization retain ground and dropping.
inline constexpr uint32_t OnGround = 3, Dropping = 5;
inline constexpr int GoldStat = 14; // ItemStatCost.txt 'gold'; also native 0x370C90
inline constexpr uint32_t Death = 0, Dead = 17; // native player modes
inline constexpr size_t MaxRooms = 64, MaxUnits = 4096, MaxTableRows = 65536, MaxTypeDepth = 16;
inline constexpr double PositionScale = 65536; // native unsigned 16.16 position
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
inline constexpr Evidence Checks[] = {
	{0x8B2D0, 7, 0x512D0A09BEF62F41ULL},
	{0x9A480, 165, 0xD7C8BF425C24EF8FULL},
	{0x2EFD90, 8, 0x92C8BD7E0C6E60FCULL},
	{0x34B4A0, 59, 0x066D901396954A18ULL},
	{0x2EFDE0, 13, 0x0746FEFCB398B135ULL},
	{0x34B440, 90, 0xCDDA66B522181150ULL},
	{0x266690, 249, 0xF8F63F43FFCDF8A8ULL},
	{0x36CF60, 113, 0x2C1858B9E195D24AULL},
	{0x314110, 105, 0xB84E3C95C1E6E866ULL},
	{0x300A90, 67, 0xB8B977E3BE437A28ULL},
	{0x9A1B0, 651, 0x52805E57958E13F6ULL},
	{0x293CC0, 2207, 0x6562D97AB98D2192ULL},
	{0x34A0E0, 47, 0x17D168C4E18BD823ULL},
	{0x2F5020, 147, 0xB36351CAA5312D64ULL},
};
}
