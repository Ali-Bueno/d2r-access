#pragma once
#include <cstddef>
#include <cstdint>

// D2R 3.3.93787. Read-only level-room/preset traversal, independently recovered
// from the native functions below. See docs/distant-exits.md for evidence.
namespace D2RAccess::LevelExitLayout {
inline constexpr size_t LevelFirstRoom = 0x10, RoomNext = 0x48;
// The level's own tile rect, which the game builds as the union of its rooms'
// rects (Ghidra re/ghidra, DRLG_GetLevel RVA 0x3267C0, rect writers 0x327650 / 0x3EC820).
inline constexpr size_t LevelTileX = 0x24, LevelTileY = 0x28, LevelTileWidth = 0x2C, LevelTileHeight = 0x30;
// Room1 of a RoomEx while the game keeps it streamed, set and cleared by
// 0x361290 / 0x361260; null once the room is unloaded.
inline constexpr size_t RoomActive = 0x58;
// Near logical rooms, built and sorted by 0x3614C0; the count is a size_t the
// game's own getter 0x360770 narrows to 32 bits. Copied to Room1 by 0x360610.
inline constexpr size_t RoomNear = 0x10, RoomNearCount = 0x18;
inline constexpr size_t RoomTileX = 0x60, RoomTileY = 0x64;
inline constexpr size_t RoomTileWidth = 0x68, RoomTileHeight = 0x6C;
inline constexpr size_t RoomPresets = 0x98; // 0x361040, reached through 0x2EFD20
// 0x99650 / 0x4926B0: class, room-relative subtile X/Y, next node and unit type.
inline constexpr size_t PresetClass = 4, PresetX = 8, PresetNext = 0x10;
inline constexpr size_t PresetType = 0x20, PresetY = 0x24;
inline constexpr int SubtilesPerTile = 5; // 0x326480 calls the converter 0x334ED0
// Work bounds for corrupt lists; independent of scan distance and active rooms.
inline constexpr size_t MaxRooms = 1024, MaxPresets = 16384;
}
