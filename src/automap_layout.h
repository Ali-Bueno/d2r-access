#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// D2R 3.3.93787 automap layer and cell layout, recovered offline from the
// automap code of the live image. Provenance per value, the reveal rules and
// the live validation plan: docs/automap-reader.md.
namespace D2RAccess::AutomapLayout {
// Globals. The current layer is written only by the layer switch 0xD1710
// (at 0xD18B6) and is what the renderer 0xD2600 draws; 0xD5360 walks the
// list from its head. A nonzero reveal byte makes 0xD6550 / 0xD6680 add every
// cell regardless of discovery (toggled at 0xD42E8, the loader's reveal cheat).
inline constexpr uintptr_t LayerListHead = 0x2A2CF60, CurrentLayer = 0x2A2CF68, RevealAll = 0x2A2ADD4;
// LevelDefs in the table bank (accessor 0x32C200): row +0x08 is the levels.txt
// Layer column, the argument 0xD2240 / 0xD4310 pass to InitAutomapLayer 0xD5360.
inline constexpr size_t LevelDefs = 0x14A0, LevelDefCount = 0x14A8, LevelDefStride = 0x9C, LevelDefLayer = 0x08;
// One .ma save slot per layer: the loader 0xD5710 reads a 100-entry offset table.
inline constexpr int32_t LayerSlots = 100;
// Layer (0xB0 bytes, built by 0xD5360): number, town-map sheet (0xD22F0 sets
// 1..3), four cell trees (floors, walls, objects, extras) and the list link.
inline constexpr size_t LayerNumber = 0x00, LayerSheet = 0x04, LayerNext = 0xA8;
inline constexpr std::array<size_t, 4> LayerTrees{0x08, 0x30, 0x58, 0x80};
// Tree header (0xD5360 init, 0xD4D60 clear, 0xD1460 insert): an empty tree has
// no root and both extremes pointing back at the header.
inline constexpr size_t TreeRoot = 0x00, TreeLeftmost = 0x08, TreeRightmost = 0x10, TreeNodeCount = 0x20;
// Red-black node (0x30 bytes, 0xD1460), ordered by (y, x, cell priority) in
// 0xD4B70. The root's parent is the header; absent children are null.
inline constexpr size_t NodeParent = 0x00, NodeLeft = 0x08, NodeRight = 0x10,
	NodeOrigin = 0x20, NodeCell = 0x22, NodeX = 0x24, NodeY = 0x28, NodeSize = 0x30;
// Node origin: 0 revealed this session (0xD5160, 0xD52B0), 1 restored from the
// .ma save (0xD5710); only origin 0 is written back by the saver 0xD7CE0.
inline constexpr int16_t OriginSession = 0, OriginRestored = 1;
// Floor/wall cell pixels (0xD5160 via 0x334EF0, then /10): 8(tx-ty), 4(tx+ty);
// a tile type >= 16 (lower wall) is drawn 24 pixels lower.
inline constexpr int32_t TileStepX = 8, TileStepY = 4, LowerWallType = 16, LowerWallDrop = 24;
// Unit cell pixels (0xD52B0): client pixel / 10, then +1 and -3.
inline constexpr int32_t ClientPerPixel = 10, UnitBiasX = 1, UnitBiasY = -3;
// Client pixels of a subtile (static 0x34D540 -> 0x334E00, dynamic 0x3818E0 ->
// 0x334DE0): 16(x-y), 8(x+y). The game's own inverse is 0x334E80.
inline constexpr int32_t ClientStepX = 16, ClientStepY = 8;
// objects.txt AutoMap cell, read by the object reveal 0xD6680 at 0xD6733.
inline constexpr size_t ObjectAutoMap = 0x164;
// Work bounds for a corrupt or half-updated tree; a red-black tree of
// MaxNodes cells is at most 2*log2(n+1) = 40 levels deep.
inline constexpr uint64_t MaxNodes = uint64_t{1} << 19;
inline constexpr size_t MaxDepth = 64;

struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
// FNV-1a of each complete function, hashed with LayoutCheck::Hash over the
// live code (re/D2R.rebuilt.exe, cross-checked against a second live dump).
inline constexpr Evidence Checks[] = {
	{0xD5360, 228, 0x8FAF295454B2C5B6ULL},  // InitAutomapLayer: list head, +0xA8 link, 0xB0 layer, tree headers
	{0xD1710, 553, 0x748729ED5E8EA7F3ULL},  // layer switch: save, clear four trees, write current layer, load
	{0xD1460, 233, 0xB8E13FBCD1B6F700ULL},  // tree insert: 0x30 node, parent/left/right, count +0x20
	{0xD4B70, 493, 0xE0063CDAEE03657EULL},  // insert position: key (y +0x28, x +0x24, cell +0x22)
	{0xD4D60, 107, 0x4A543EBA24FC4AE0ULL},  // tree clear: empty header shape
	{0xD5160, 334, 0x64CE8EE57B9DBCE5ULL},  // floor/wall cell: tile -> pixel, lower-wall drop, origin 0
	{0xD52B0, 168, 0x50B581379A1622B2ULL},  // unit cell: client / 10, +1 / -3, origin 0
	{0xD6550, 297, 0x5FDEB369CF76BB00ULL},  // RevealAutomapRoom: floor/wall passes, reveal byte
	{0xD6680, 660, 0x5D075004D17CA856ULL},  // object pass: unit bits, objects.txt +0x164, exceptions
	{0x32C200, 246, 0x943F2C1529CB17D2ULL}, // LevelDefs row: bank +0x14A0 / +0x14A8, stride 0x9C
	{0x334EF0, 35, 0xFA841E98BB1C1EFCULL},  // tile -> client pixels
	{0x334E80, 29, 0x43364C815BD95622ULL},  // client pixels -> subtile (the game's inverse)
	{0x34AF60, 77, 0x12069A74134BCDDFULL},  // unit client X: path +0x08
	{0x34AFB0, 77, 0x15A58BBDC556E0DBULL},  // unit client Y: path +0x0C
	{0xD4310, 583, 0x71F0D4E407787591ULL},  // updater: throttle, LevelDefs Layer, near-room reveal
	{0xD5710, 2246, 0xE03806FB818930F6ULL}, // .ma loader: four groups, origin 1
	{0x34D540, 115, 0x739E5EE8B9101DE5ULL}, // static path: subtile at +0x10/+0x14, client at +0x08/+0x0C
	{0x334E00, 27, 0x1B2DB48CB7B17318ULL},  // subtile -> client pixels
	{0x334DE0, 26, 0x946AEF40BA204C59ULL},  // dynamic path: 32nds of a subtile -> client pixels
};

// The tiles the automap would draw, for the explored share (docs/automap-reader.md, "Drawable tiles").
// An active room's tile block (Room +0x08, built by 0x2EF340): walls and floors, each a pointer and a count.
inline constexpr size_t RoomTileBlock = 0x08, BlockWalls = 0x00, BlockWallCount = 0x08, BlockFloors = 0x20, BlockFloorCount = 0x28;
// Tile record (0xD5160): tile relative to the room, flags, tile-library record, tile type.
inline constexpr size_t TileStride = 0x48, TileX = 0x08, TileY = 0x0C, TileFlags = 0x18, TileLibrary = 0x20, TileType = 0x28;
// Flag 0x8: never drawn (0xD6550). The floor pass 0x21CB40 shows only floors of draw layer 1-2
// (flag bits 14-16) and library orientation 0, so no other floor is ever revealed.
inline constexpr uint32_t TileHidden = 0x8, TileLayerShift = 14, TileLayerMask = 0x7, FloorLayerFirst = 1, FloorLayerLast = 2;
inline constexpr uint32_t FloorOrientation = 0;
// Tile library (the DT1 header): orientation, main index (AutoMap Style), sub index (Sequence).
inline constexpr size_t LibraryKeys = 0x14; // three uint32: orientation, style, sequence
// LevelDefs row +0x34: Levels.txt LevelType, the AutoMap.txt key (0x3269B0).
inline constexpr size_t LevelDefType = 0x34;
// AutoMap.txt as loaded by 0x32CAE0 and read by 0x32BDF0: a static vector of 0x20-byte rows and,
// per level type, the {first, end} range of its rows (end exclusive; first -1: none).
inline constexpr uintptr_t MapRows = 0x2A9A8C8, MapRowCount = 0x2A9A8D0, MapRanges = 0x2A9A8E0;
inline constexpr int32_t MapLevelTypes = 36;
inline constexpr size_t MapRowStride = 0x20, MapRowOrientation = 0x04, MapRowStyle = 0x08, MapRowFirstSequence = 0x09,
	MapRowLastSequence = 0x0A, MapRowCel = 0x0C, MapRowCels = 0x1C;
// Bounds for a corrupt read: one active room's tile list, one level type's rows.
inline constexpr uint64_t MaxRoomTiles = 4096, MaxTypeRows = 1024;

inline constexpr Evidence DrawableChecks[] = {
	{0x32BDF0, 871, 0xC031EEC4C5687834ULL},  // AutoMap lookup: the globals, the row match, -1 for no cell
	{0x32CAE0, 1655, 0x6C0E84E5C4DE8690ULL}, // AutoMap loader: row fields, cel count +0x1C, 36 ranges
	{0x2EFB70, 116, 0x709027C9B95A4DFEULL},  // floors: block +0x20 / +0x28
	{0x2EFDF0, 81, 0xC9F6569597E11766ULL},   // walls: block +0x00 / +0x08
	{0x1460210, 51, 0xB00F61F2FA029B02ULL},  // library orientation +0x14
	{0x1460190, 51, 0x5C5777AD0AFC1012ULL},  // library main index +0x18
	{0x1460150, 51, 0x2AA8A3B5F029B132ULL},  // library sub index +0x1C
	{0x3269B0, 17, 0x0D58E1A6DEBF99A9ULL},   // LevelDefs +0x34
};
}
