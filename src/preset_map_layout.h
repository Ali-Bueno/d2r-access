#pragma once
#include <cstddef>
#include <cstdint>

// The LvlPrest map a logical room was cut from (D2R 3.3, Ghidra re/ghidra D2R_live).
// docs/places.md#the-map-under-the-player
namespace D2RAccess::PresetMapLayout {
// RoomEx type, written by the allocator 0x361140 (2nd argument); 1 is an outdoor
// filler cell (0x3F93A0), 2 a room cut from a preset map, mazes included.
inline constexpr size_t RoomType = 0x74, RoomData = 0x40;
inline constexpr int32_t PresetRoomType = 2;
// Preset room data (0x3DCB20 / 0x3DDE30): the map it was cut from.
inline constexpr size_t PresetRoomMap = 0x08;
// DrlgMap, allocated by 0x3DD1C0: LvlPrest id, picked File index, record,
// origin and size in the tiles of RoomEx+0x60 (the debug overlay 0x962A0
// prints playerSubtile/5 - origin).
inline constexpr size_t MapDef = 0x00, MapFile = 0x04, MapRecord = 0x08;
inline constexpr size_t MapTileX = 0x18, MapTileY = 0x1C, MapTileWidth = 0x20, MapTileHeight = 0x24;
// LvlPrest record: File1..File6 inline, 60 bytes each (getter 0x360FD0 -> 0x3DDFB0).
inline constexpr size_t RecordFiles = 0x40, RecordFileStride = 0x3C;
inline constexpr int32_t MaxFiles = 6;

struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
inline constexpr Evidence Checks[] = {
	{0x360FD0, 18, 0xD070522CFA8B4097ULL},  // RoomEx+0x74 == 2 (preset), contiguous part only
	{0x3DDFB0, 28, 0x26AEA5D37AB66B05ULL},  // RoomEx+0x40 -> +0x08 map; map+0x04 file, +0x08 record; rec+0x40+i*0x3C
	{0x361140, 275, 0xFC54AF661CE8B988ULL}, // RoomEx allocator: +0x74 type, +0x40 data
	{0x3DD1C0, 361, 0xA2CA257091296A60ULL}, // DrlgMap: def, file, record, origin/size +0x18..+0x24
	{0x3DDE30, 209, 0x467380D7D63AF3D4ULL}, // preset room data: +0x08 map
	{0x192B20, 5, 0x718E401FF86D81A2ULL},   // Room+0x18 -> RoomEx
};
}
