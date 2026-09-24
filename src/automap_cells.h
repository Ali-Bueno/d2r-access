#pragma once
#include "native_rooms.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// The cells the native automap has drawn for its current layer, read without
// calling the game, and the game's own pixel conversions run backwards. Pure
// logic over a memory reader the tests can fake; the live reader passes
// NativeMemory::Copy. Layout and provenance: automap_layout.h.
namespace D2RAccess::AutomapCells {

enum class Tree : uint8_t { Floor, Wall, Object, Extra };
inline constexpr size_t TreeCount = 4;
// Session cells were revealed in this game; restored ones came from the .ma save.
enum class Origin : uint8_t { Session, Restored, Other };

struct Cell {
	Tree    tree = Tree::Floor;
	Origin  origin = Origin::Session;
	int16_t number = 0;             // AutoMap.txt Cel, or objects.txt AutoMap for objects
	int32_t pixelX = 0, pixelY = 0; // as stored: client pixels / 10
	bool    lattice = false;        // floors/walls: the pixels lie on the tile lattice
	int     x = 0, y = 0;           // floors/walls: tile; objects/extras: nearest subtile
};

struct Point { int32_t x = 0, y = 0; };
struct Position { double x = 0, y = 0; };

// Forward conversions, exactly as the game builds cells.
Point TilePixel(int tileX, int tileY, int tileType);
Point ClientPixel(int32_t clientX, int32_t clientY); // a unit cell
Point StaticClient(int subtileX, int subtileY);      // a static unit's client pixels

// Exact inverse of TilePixel for ordinary tiles; false off the lattice (the
// /16 check). A lower-wall cell decodes three tiles further along both axes.
bool PixelTile(Point pixel, int& tileX, int& tileY);
// Inverse of ClientPixel then the game's 0x334E80, from the middle of each
// division interval: exact after rounding for a static unit's whole subtile.
Position UnitSubtile(Point pixel);
// The same without the unit bias, for the town-map extras.
Position PixelSubtile(Point pixel);
void Decode(Cell& cell);

// Copies `size` bytes at `address`; false when unreadable.
using ReadMemory = bool (*)(uintptr_t address, void* output, size_t size) noexcept;

enum class TreeStatus : uint8_t { Ok, Unreadable, TooLarge, Corrupt };
struct Limits { uint64_t maxNodes; size_t maxDepth; };

// In-order walk of one tree; appends nothing unless the whole tree is
// consistent: every parent link, the header's count and extremes, and the key order.
TreeStatus ReadTree(ReadMemory read, uintptr_t header, Tree tree, const Limits& limits, std::vector<Cell>& output);

enum class LayerStatus : uint8_t { Ok, NoLayer, Unreadable, Mismatch };
struct Layer {
	int32_t                           number = -1, sheet = 0;
	std::array<uint64_t, TreeCount>   counts{};   // native header counts
	std::array<TreeStatus, TreeCount> trees{};
};
// Reads the layer's header, refusing one whose number is not `expected`: the
// game keeps only the current layer's cells resident. `walk` also reads cells.
LayerStatus ReadLayer(ReadMemory read, uintptr_t layer, int32_t expected, bool walk, const Limits& limits,
	Layer& output, std::vector<Cell>& cells);

// Whether a decoded cell lies in one of the level's rooms. Layers are shared
// between levels, so this is what separates the current level's cells.
bool InRooms(const Cell& cell, const std::vector<NativeRooms::Room>& rooms);
// The rooms holding at least one floor or wall cell: ground the automap has drawn.
std::vector<NativeRooms::Room> RevealedRooms(const std::vector<Cell>& cells, const std::vector<NativeRooms::Room>& rooms);

const char* TreeName(Tree tree);
const char* StatusName(TreeStatus status);

}
