#include "automap_cells.h"
#include "automap_layout.h"
#include "test_support.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

using namespace D2RAccess;
using namespace AutomapLayout;
using AutomapCells::Cell, AutomapCells::Origin, AutomapCells::Tree, AutomapCells::TreeStatus, AutomapCells::LayerStatus;

namespace {
// Native memory at made-up addresses, so a test can point anywhere, including nowhere.
struct FakeMemory {
	std::map<uintptr_t, std::vector<uint8_t>> regions;
	void Map(uintptr_t base, size_t size) { regions[base].assign(size, 0); }
	template<class T> void Put(uintptr_t address, T value) {
		auto it = regions.upper_bound(address);
		--it;
		std::memcpy(it->second.data() + (address - it->first), &value, sizeof(value));
	}
	bool Read(uintptr_t address, void* output, size_t size) const {
		auto it = regions.upper_bound(address);
		if (it == regions.begin()) return false;
		--it;
		if (address + size > it->first + it->second.size()) return false;
		std::memcpy(output, it->second.data() + (address - it->first), size);
		return true;
	}
};
FakeMemory g_memory;
bool ReadFake(uintptr_t address, void* output, size_t size) noexcept { return g_memory.Read(address, output, size); }

struct Key { int16_t origin, cell; int32_t x, y; };
constexpr uintptr_t LayerAddress = 0x10000;
constexpr AutomapCells::Limits Wide{MaxNodes, MaxDepth};

// A balanced tree built the way the game links one: nodes sorted by (y, x),
// the root's parent is the header, absent children are null.
struct Built { uintptr_t header; std::vector<uintptr_t> nodes; };
uintptr_t Link(const std::vector<Key>& keys, std::vector<uintptr_t>& nodes, size_t lo, size_t hi, uintptr_t parent) {
	if (lo >= hi) return 0;
	const size_t mid = (lo + hi) / 2;
	const uintptr_t node = nodes[mid];
	g_memory.Put(node + NodeParent, parent);
	g_memory.Put(node + NodeLeft, Link(keys, nodes, lo, mid, node));
	g_memory.Put(node + NodeRight, Link(keys, nodes, mid + 1, hi, node));
	g_memory.Put(node + NodeOrigin, keys[mid].origin);
	g_memory.Put(node + NodeCell, keys[mid].cell);
	g_memory.Put(node + NodeX, keys[mid].x);
	g_memory.Put(node + NodeY, keys[mid].y);
	return node;
}
Built Build(uintptr_t header, std::vector<Key> keys, uintptr_t nodeBase) {
	std::stable_sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
	Built built{header, {}};
	for (size_t i = 0; i < keys.size(); ++i) {
		built.nodes.push_back(nodeBase + i * NodeSize);
		g_memory.Map(built.nodes.back(), NodeSize);
	}
	const uintptr_t root = Link(keys, built.nodes, 0, keys.size(), header);
	g_memory.Put(header + TreeRoot, root);
	g_memory.Put(header + TreeLeftmost, keys.empty() ? header : built.nodes.front());
	g_memory.Put(header + TreeRightmost, keys.empty() ? header : built.nodes.back());
	g_memory.Put<uint64_t>(header + TreeNodeCount, keys.size());
	return built;
}
std::vector<Key> Floors(int count) {
	std::vector<Key> keys;
	for (int i = 0; i < count; ++i) {
		const auto pixel = AutomapCells::TilePixel(20 + i % 7, 30 + i / 7, 0);
		keys.push_back({static_cast<int16_t>(i % 2), static_cast<int16_t>(100 + i), pixel.x, pixel.y});
	}
	return keys;
}
uintptr_t Header(Tree tree) { return LayerAddress + LayerTrees[static_cast<size_t>(tree)]; }
void FreshLayer(int32_t number) {
	g_memory.regions.clear();
	g_memory.Map(LayerAddress, 0xB0);
	g_memory.Put(LayerAddress + LayerNumber, number);
	g_memory.Put<int32_t>(LayerAddress + LayerSheet, 0);
	for (size_t i = 0; i < AutomapCells::TreeCount; ++i) Build(Header(static_cast<Tree>(i)), {}, 0);
}

void Conversions() {
	const auto pixel = AutomapCells::TilePixel(10, 4, 0);
	Check(pixel.x == 48 && pixel.y == 56, "a floor tile lands at 8(tx-ty), 4(tx+ty) automap pixels");
	bool exact = true;
	for (int tx = 0; tx < 400; tx += 3)
		for (int ty = 0; ty < 400; ty += 5) {
			int x = -1, y = -1;
			exact = exact && AutomapCells::PixelTile(AutomapCells::TilePixel(tx, ty, 0), x, y) && x == tx && y == ty;
		}
	Check(exact, "the tile inverse is exact across the map, including tiles left of the diagonal");
	int x = 0, y = 0;
	Check(AutomapCells::PixelTile(AutomapCells::TilePixel(10, 4, LowerWallType), x, y) && x == 13 && y == 7,
		"a lower wall (type >= 16) is 24 pixels lower, so it decodes three tiles further on both axes");
	Check(AutomapCells::TilePixel(10, 4, LowerWallType - 1).y == pixel.y, "tile types below 16 are not dropped");
	Check(!AutomapCells::PixelTile({1, 0}, x, y) && !AutomapCells::PixelTile({8, 0}, x, y) && !AutomapCells::PixelTile({0, 2}, x, y),
		"pixels off the tile lattice fail the /16 check");

	const auto client = AutomapCells::StaticClient(100, 60);
	Check(client.x == 640 && client.y == 1280, "a static unit's client pixels are 16(x-y), 8(x+y)");
	const auto negative = AutomapCells::ClientPixel(-15, 25);
	Check(negative.x == 0 && negative.y == -1, "unit cells truncate toward zero before the +1 / -3 bias");
	bool staticExact = true;
	double dynamicError = 0;
	for (int sx = 0; sx < 2000; sx += 7)
		for (int sy = 0; sy < 2000; sy += 11) {
			const auto c = AutomapCells::StaticClient(sx, sy);
			const auto p = AutomapCells::UnitSubtile(AutomapCells::ClientPixel(c.x, c.y));
			staticExact = staticExact && std::lround(p.x) == sx && std::lround(p.y) == sy;
			// A walking unit: 32nds of a subtile, as the dynamic path keeps them.
			// Level positions are never within a few subtiles of the world origin.
			if (sx < 10 || sy < 10) continue;
			const double fx = sx + 0.40625, fy = sy + 0.78125;
			const auto d = AutomapCells::UnitSubtile(AutomapCells::ClientPixel(
				static_cast<int32_t>(std::floor((fx - fy) * ClientStepX)), static_cast<int32_t>(std::floor((fx + fy) * ClientStepY))));
			dynamicError = std::max({dynamicError, std::abs(d.x - fx), std::abs(d.y - fy)});
		}
	Check(staticExact, "a static object's cell rounds back to its exact subtile, on and off the diagonal");
	// Flooring to client pixels and the division by ten bound the error at (10 + 2 * 5.5) / 32.
	Check(dynamicError < 0.66, "a moving unit's cell decodes within two thirds of a subtile");
	const auto corner = AutomapCells::PixelSubtile(AutomapCells::TilePixel(12, 9, 0));
	Check(corner.x == 60 && corner.y == 45, "the unbiased inverse maps a tile's pixels to its corner subtile");

	Cell floor{Tree::Floor, Origin::Session, 3, pixel.x, pixel.y};
	AutomapCells::Decode(floor);
	Check(floor.lattice && floor.x == 10 && floor.y == 4, "a floor cell decodes to its tile");
	const auto stone = AutomapCells::ClientPixel(AutomapCells::StaticClient(4700, 5210).x, AutomapCells::StaticClient(4700, 5210).y);
	Cell object{Tree::Object, Origin::Session, 314, stone.x, stone.y};
	AutomapCells::Decode(object);
	Check(!object.lattice && object.x == 4700 && object.y == 5210, "an object cell decodes to its unit's subtile");
}

void Walks() {
	FreshLayer(0);
	std::vector<Cell> cells;
	Check(AutomapCells::ReadTree(ReadFake, Header(Tree::Floor), Tree::Floor, Wide, cells) == TreeStatus::Ok && cells.empty(),
		"an empty tree reads as no cells");
	g_memory.Put<uint64_t>(Header(Tree::Floor) + TreeNodeCount, 1);
	Check(AutomapCells::ReadTree(ReadFake, Header(Tree::Floor), Tree::Floor, Wide, cells) == TreeStatus::Corrupt,
		"an empty tree that claims a cell is corrupt");

	auto keys = Floors(50);
	keys[3].origin = 7;
	auto built = Build(Header(Tree::Floor), keys, 0x100000);
	Check(AutomapCells::ReadTree(ReadFake, Header(Tree::Floor), Tree::Floor, Wide, cells) == TreeStatus::Ok && cells.size() == 50,
		"every node of a consistent tree is read");
	bool ordered = true, decoded = true;
	size_t session = 0, restored = 0, other = 0;
	for (size_t i = 0; i < cells.size(); ++i) {
		if (i && (cells[i].pixelY < cells[i - 1].pixelY || (cells[i].pixelY == cells[i - 1].pixelY && cells[i].pixelX < cells[i - 1].pixelX))) ordered = false;
		decoded = decoded && cells[i].lattice && cells[i].tree == Tree::Floor;
		session += cells[i].origin == Origin::Session;
		restored += cells[i].origin == Origin::Restored;
		other += cells[i].origin == Origin::Other;
	}
	Check(ordered && decoded, "cells come out in the game's (y, x) order, decoded to tiles");
	Check(session == 25 && restored == 24 && other == 1, "origin 0 is a session reveal, 1 a restored save, anything else is reported apart");

	cells.assign(1, Cell{});
	Check(AutomapCells::ReadTree(ReadFake, Header(Tree::Floor), Tree::Floor, {10, MaxDepth}, cells) == TreeStatus::TooLarge && cells.size() == 1,
		"a tree above the node cap is refused before any node is read");
	Check(AutomapCells::ReadTree(ReadFake, Header(Tree::Floor), Tree::Floor, {MaxNodes, 3}, cells) == TreeStatus::Corrupt && cells.size() == 1,
		"a tree deeper than the depth cap fails closed and keeps earlier cells");

	const auto corrupt = [&](const char* what) {
		cells.assign(1, Cell{});
		Check(AutomapCells::ReadTree(ReadFake, Header(Tree::Floor), Tree::Floor, Wide, cells) == TreeStatus::Corrupt && cells.size() == 1, what);
		built = Build(Header(Tree::Floor), keys, 0x100000);
	};
	uintptr_t root = 0;
	g_memory.Read(Header(Tree::Floor) + TreeRoot, &root, sizeof(root));
	g_memory.Put(built.nodes.back() + NodeRight, root);
	corrupt("a child link back to the root is a cycle and yields nothing");
	g_memory.Put(built.nodes[10] + NodeParent, built.nodes[40]);
	corrupt("a node that names the wrong parent fails closed");
	g_memory.Put<uint64_t>(Header(Tree::Floor) + TreeNodeCount, 49);
	corrupt("more nodes than the header counts fails closed");
	g_memory.Put<uint64_t>(Header(Tree::Floor) + TreeNodeCount, 51);
	corrupt("fewer nodes than the header counts fails closed");
	g_memory.Put(Header(Tree::Floor) + TreeLeftmost, built.nodes[1]);
	corrupt("a header whose leftmost node is not the first in order fails closed");
	g_memory.Put(built.nodes[20] + NodeY, int32_t{-1000});
	corrupt("keys out of the game's order fail closed");

	// A chain hanging to the left, the deepest shape a valid tree can take here.
	std::vector<uintptr_t> chain;
	for (int i = 0; i < 20; ++i) { chain.push_back(0x300000 + i * NodeSize); g_memory.Map(chain.back(), NodeSize); }
	for (size_t i = 0; i < chain.size(); ++i) {
		g_memory.Put(chain[i] + NodeParent, i ? chain[i - 1] : Header(Tree::Wall));
		g_memory.Put(chain[i] + NodeLeft, i + 1 < chain.size() ? chain[i + 1] : uintptr_t{0});
		g_memory.Put(chain[i] + NodeRight, uintptr_t{0});
		g_memory.Put<int32_t>(chain[i] + NodeY, 1000 - static_cast<int32_t>(i) * 4);
	}
	g_memory.Put(Header(Tree::Wall) + TreeRoot, chain.front());
	g_memory.Put(Header(Tree::Wall) + TreeLeftmost, chain.back());
	g_memory.Put(Header(Tree::Wall) + TreeRightmost, chain.front());
	g_memory.Put<uint64_t>(Header(Tree::Wall) + TreeNodeCount, chain.size());
	cells.clear();
	Check(AutomapCells::ReadTree(ReadFake, Header(Tree::Wall), Tree::Wall, {MaxNodes, 8}, cells) == TreeStatus::Corrupt && cells.empty(),
		"the depth cap bounds a degenerate chain");
	Check(AutomapCells::ReadTree(ReadFake, Header(Tree::Wall), Tree::Wall, Wide, cells) == TreeStatus::Ok && cells.size() == 20,
		"the same chain within the cap reads completely");
	g_memory.Put(chain[5] + NodeLeft, uintptr_t{0x7FFF0000});
	cells.clear();
	Check(AutomapCells::ReadTree(ReadFake, Header(Tree::Wall), Tree::Wall, Wide, cells) == TreeStatus::Unreadable && cells.empty(),
		"a child in unmapped memory yields nothing, not a partial tree");
	Check(AutomapCells::ReadTree(ReadFake, 0x7FFF0000, Tree::Wall, Wide, cells) == TreeStatus::Unreadable, "an unreadable header is reported");
}

void Layers() {
	FreshLayer(27);
	Build(Header(Tree::Floor), Floors(12), 0x100000);
	const auto stone = AutomapCells::ClientPixel(AutomapCells::StaticClient(120, 170).x, AutomapCells::StaticClient(120, 170).y);
	Build(Header(Tree::Object), {{0, 314, stone.x, stone.y}, {1, 307, stone.x + 40, stone.y + 40}}, 0x200000);
	AutomapCells::Layer layer;
	std::vector<Cell> cells{Cell{}};
	Check(AutomapCells::ReadLayer(ReadFake, LayerAddress, 27, true, Wide, layer, cells) == LayerStatus::Ok, "the current layer reads when its number matches");
	Check(layer.number == 27 && layer.counts[0] == 12 && layer.counts[2] == 2 && layer.counts[1] == 0 && cells.size() == 14,
		"counts come from every tree header and cells from every tree, replacing earlier output");
	Check(std::all_of(layer.trees.begin(), layer.trees.end(), [](TreeStatus s) { return s == TreeStatus::Ok; }), "every tree reports ok");
	const auto found = std::find_if(cells.begin(), cells.end(), [](const Cell& c) { return c.number == 314; });
	Check(found != cells.end(), "the object cell is read");
	const Cell object = *found;
	Check(object.tree == Tree::Object && object.x == 120 && object.y == 170 && object.origin == Origin::Session,
		"an object cell carries its tree, cell number, origin and subtile");

	Check(AutomapCells::ReadLayer(ReadFake, LayerAddress, 0, true, Wide, layer, cells) == LayerStatus::Mismatch && cells.empty() && layer.number == 27,
		"another level's layer is refused: only the current layer's cells are resident");
	Check(AutomapCells::ReadLayer(ReadFake, LayerAddress, 27, false, Wide, layer, cells) == LayerStatus::Ok && cells.empty() && layer.counts[0] == 12,
		"a status read takes only the header counts");
	Check(AutomapCells::ReadLayer(ReadFake, 0, 27, true, Wide, layer, cells) == LayerStatus::NoLayer, "no current layer is its own state");
	Check(AutomapCells::ReadLayer(ReadFake, 0x7FFF0000, 27, true, Wide, layer, cells) == LayerStatus::Unreadable, "an unreadable layer is reported");

	g_memory.Put<uint64_t>(Header(Tree::Wall) + TreeNodeCount, 5);
	Check(AutomapCells::ReadLayer(ReadFake, LayerAddress, 27, true, Wide, layer, cells) == LayerStatus::Ok
		&& layer.trees[1] == TreeStatus::Corrupt && layer.trees[0] == TreeStatus::Ok && cells.size() == 14,
		"one corrupt tree is isolated; the others still read");

	const std::vector<NativeRooms::Room> rooms{{20, 30, 7, 2, 1}, {24, 34, 2, 2, 2}};
	const auto inRooms = std::count_if(cells.begin(), cells.end(), [&](const Cell& c) { return c.tree == Tree::Floor && AutomapCells::InRooms(c, rooms); });
	Check(inRooms == 12, "floor tiles inside the level's room rectangles are the level's");
	Check(!AutomapCells::InRooms(object, {rooms[0]}), "an object outside every room is not the level's");
	Check(AutomapCells::InRooms(object, {{24, 34, 1, 1, 1}}), "objects are tested in subtiles, five per tile");
	Cell offLattice{Tree::Floor, Origin::Session, 0, 1, 0};
	AutomapCells::Decode(offLattice);
	Check(!AutomapCells::InRooms(offLattice, {{-100, -100, 400, 400, 1}}), "a floor cell off the lattice belongs to no room");
}
}

void Revealed() {
	const std::vector<NativeRooms::Room> rooms{{0, 0, 8, 8, 1}, {8, 0, 8, 8, 2}, {16, 0, 8, 8, 3}};
	auto floor = [](int x, int y) { Cell c{Tree::Floor}; c.x = x; c.y = y; c.lattice = true; return c; };
	Cell wall = floor(20, 3);
	wall.tree = Tree::Wall;
	Cell offLattice = floor(9, 1);
	offLattice.lattice = false;
	Cell object{Tree::Object};
	object.x = 12; object.y = 2;
	const auto revealed = AutomapCells::RevealedRooms({floor(7, 7), wall, offLattice, object}, rooms);
	Check(revealed.size() == 2 && revealed[0].id == 1 && revealed[1].id == 3,
		"a floor or wall cell reveals its room; off-lattice and object cells do not");
	Check(AutomapCells::RevealedRooms({}, rooms).empty(), "an empty automap reveals no room");
}

void DrawnTiles() {
	auto floor = [](int x, int y) { Cell c{Tree::Floor}; c.x = x; c.y = y; c.lattice = true; return c; };
	Cell wall = floor(9, 1);
	wall.tree = Tree::Wall;
	Cell off = floor(2, 0);
	off.lattice = false;
	Cell object{Tree::Object};
	object.x = 3; object.y = 0;
	const std::vector<std::vector<AutomapCells::Point>> rooms{{{0, 0}, {1, 0}, {2, 0}, {3, 0}}, {{8, 1}, {9, 1}}, {}};
	const auto drawn = AutomapCells::DrawnTiles({floor(0, 0), floor(1, 0), floor(5, 5), wall, off, object}, rooms);
	Check(drawn.size() == 3 && drawn[0] == 2 && drawn[1] == 1 && drawn[2] == 0,
		"only drawn floor and wall cells on a listed tile count; off-lattice, object and unlisted cells do not");
}

// The game's AutoMap lookup (0x32BDF0): the first matching row decides, 0xFF matches anything.
void Cells() {
	using AutomapCells::MapRow;
	const std::vector<MapRow> rows{
		{1, 3, 0, 2, AutomapCells::NoCell, 0}, // wall style 3, sequences 0-2: no cell
		{1, AutomapCells::MapAny, AutomapCells::MapAny, 0, 7, 1},
		{0, 5, 4, 4, 12, 2},
	};
	Check(!AutomapCells::HasCell(rows, 1, 3, 1), "the first row that matches decides, even without a cell");
	Check(AutomapCells::HasCell(rows, 1, 3, 3) && AutomapCells::HasCell(rows, 1, 9, 0), "past its sequences the any-style row gives one");
	Check(AutomapCells::HasCell(rows, 0, 5, 4) && !AutomapCells::HasCell(rows, 0, 5, 5) && !AutomapCells::HasCell(rows, 0, 6, 4),
		"a floor needs its style and a sequence in range");
	Check(!AutomapCells::HasCell(rows, 2, 0, 0) && !AutomapCells::HasCell({}, 0, 0, 0), "an orientation without a row has no cell");
	const std::vector<MapRow> bare{{0, AutomapCells::MapAny, AutomapCells::MapAny, 0, 40, 0}};
	Check(AutomapCells::HasCell(bare, 0, 1, 1), "a row without its count of cels still gives its first");
}

int main() {
	DrawnTiles();
	Cells();
	Conversions();
	Walks();
	Layers();
	Revealed();
	Passed();
}
