#pragma once
#include <cstddef>
#include <cstdint>

// D2R 3.3.93787. Read-only level-room/preset traversal, independently recovered
// from the native functions below. See docs/distant-exits.md for evidence.
namespace D2RAccess::LevelExitLayout {
inline constexpr size_t LevelFirstRoom = 0x10, RoomNext = 0x48;
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
