#include "automap_cells.h"
#include "automap_layout.h"
#include "level_exit_layout.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

namespace D2RAccess::AutomapCells {
namespace {
using namespace AutomapLayout;

template<class T> bool Get(ReadMemory read, uintptr_t address, T& value) { return read(address, &value, sizeof(T)); }

struct Node { uintptr_t parent = 0, left = 0, right = 0; int16_t origin = 0, cell = 0; int32_t x = 0, y = 0; };

bool ReadNode(ReadMemory read, uintptr_t address, Node& node) {
	uint8_t bytes[NodeY + sizeof(int32_t)]{};
	if (!read(address, bytes, sizeof(bytes))) return false;
	std::memcpy(&node.parent, bytes + NodeParent, sizeof(node.parent));
	std::memcpy(&node.left, bytes + NodeLeft, sizeof(node.left));
	std::memcpy(&node.right, bytes + NodeRight, sizeof(node.right));
	std::memcpy(&node.origin, bytes + NodeOrigin, sizeof(node.origin));
	std::memcpy(&node.cell, bytes + NodeCell, sizeof(node.cell));
	std::memcpy(&node.x, bytes + NodeX, sizeof(node.x));
	std::memcpy(&node.y, bytes + NodeY, sizeof(node.y));
	return true;
}

Origin OriginOf(int16_t value) {
	return value == OriginSession ? Origin::Session : value == OriginRestored ? Origin::Restored : Origin::Other;
}

// The middle of the client pixels a truncating division by ten maps to `quotient`.
double Middle(int64_t quotient) {
	constexpr double Half = (ClientPerPixel - 1) / 2.0;
	if (quotient == 0) return 0;
	return static_cast<double>(quotient * ClientPerPixel) + (quotient > 0 ? Half : -Half);
}

// 0x334E80: client pixels back to subtiles.
Position ClientSubtile(double clientX, double clientY) {
	constexpr double Scale = 2.0 * ClientStepX;
	return {(clientX + 2 * clientY) / Scale, (2 * clientY - clientX) / Scale};
}

bool Tiles(Tree tree) { return tree == Tree::Floor || tree == Tree::Wall; }
uint64_t TileKey(int x, int y) { return uint64_t{static_cast<uint32_t>(x)} << 32 | static_cast<uint32_t>(y); }
}

Point TilePixel(int tileX, int tileY, int tileType) {
	return {TileStepX * (tileX - tileY), TileStepY * (tileX + tileY) + (tileType >= LowerWallType ? LowerWallDrop : 0)};
}

Point ClientPixel(int32_t clientX, int32_t clientY) {
	return {clientX / ClientPerPixel + UnitBiasX, clientY / ClientPerPixel + UnitBiasY};
}

Point StaticClient(int subtileX, int subtileY) {
	return {ClientStepX * (subtileX - subtileY), ClientStepY * (subtileX + subtileY)};
}

bool PixelTile(Point pixel, int& tileX, int& tileY) {
	constexpr int64_t Lattice = 2 * TileStepX;
	const int64_t sum = int64_t{pixel.x} + 2 * int64_t{pixel.y}, difference = 2 * int64_t{pixel.y} - pixel.x;
	tileX = static_cast<int>(std::floor(static_cast<double>(sum) / Lattice));
	tileY = static_cast<int>(std::floor(static_cast<double>(difference) / Lattice));
	return sum % Lattice == 0 && difference % Lattice == 0;
}

Position UnitSubtile(Point pixel) {
	return ClientSubtile(Middle(int64_t{pixel.x} - UnitBiasX), Middle(int64_t{pixel.y} - UnitBiasY));
}

Position PixelSubtile(Point pixel) {
	return ClientSubtile(static_cast<double>(pixel.x) * ClientPerPixel, static_cast<double>(pixel.y) * ClientPerPixel);
}

void Decode(Cell& cell) {
	const Point pixel{cell.pixelX, cell.pixelY};
	if (Tiles(cell.tree)) {
		cell.lattice = PixelTile(pixel, cell.x, cell.y);
		return;
	}
	cell.lattice = false;
	const Position position = cell.tree == Tree::Object ? UnitSubtile(pixel) : PixelSubtile(pixel);
	cell.x = static_cast<int>(std::lround(position.x));
	cell.y = static_cast<int>(std::lround(position.y));
}

TreeStatus ReadTree(ReadMemory read, uintptr_t header, Tree tree, const Limits& limits, std::vector<Cell>& output) {
	uintptr_t root = 0, leftmost = 0, rightmost = 0;
	uint64_t  count = 0;
	if (!read || !header || !Get(read, header + TreeRoot, root) || !Get(read, header + TreeLeftmost, leftmost)
		|| !Get(read, header + TreeRightmost, rightmost) || !Get(read, header + TreeNodeCount, count)) return TreeStatus::Unreadable;
	if (!root) return count == 0 && leftmost == header && rightmost == header ? TreeStatus::Ok : TreeStatus::Corrupt;
	if (count > limits.maxNodes) return TreeStatus::TooLarge;

	const size_t start = output.size();
	const auto   fail = [&](TreeStatus status) { output.resize(start); return status; };
	output.reserve(start + static_cast<size_t>(count));
	struct Frame { uintptr_t address; Node node; };
	std::vector<Frame> stack;
	stack.reserve(limits.maxDepth);
	uintptr_t current = root, parent = header, last = 0;
	uint64_t  visited = 0;
	while (current || !stack.empty()) {
		// Each child must name the node it hangs from: that one link rules out
		// cycles, shared subtrees and a walk that wandered into freed memory.
		while (current) {
			Frame frame{current, {}};
			if (stack.size() >= limits.maxDepth) return fail(TreeStatus::Corrupt);
			if (!ReadNode(read, current, frame.node)) return fail(TreeStatus::Unreadable);
			if (frame.node.parent != parent) return fail(TreeStatus::Corrupt);
			parent = current;
			current = frame.node.left;
			stack.push_back(frame);
		}
		const Frame frame = stack.back();
		stack.pop_back();
		if (++visited > count || (visited == 1 && frame.address != leftmost)) return fail(TreeStatus::Corrupt);
		// 0xD4B70 orders by y, then x; equal positions differ only by cell priority.
		if (visited > 1) {
			const Cell& previous = output.back();
			if (frame.node.y < previous.pixelY || (frame.node.y == previous.pixelY && frame.node.x < previous.pixelX))
				return fail(TreeStatus::Corrupt);
		}
		Cell cell{tree, OriginOf(frame.node.origin), frame.node.cell, frame.node.x, frame.node.y};
		Decode(cell);
		output.push_back(cell);
		last = parent = frame.address;
		current = frame.node.right;
	}
	if (visited != count || last != rightmost) return fail(TreeStatus::Corrupt);
	return TreeStatus::Ok;
}

LayerStatus ReadLayer(ReadMemory read, uintptr_t layer, int32_t expected, bool walk, const Limits& limits,
	Layer& output, std::vector<Cell>& cells) {
	output = {};
	cells.clear();
	if (!layer) return LayerStatus::NoLayer;
	if (!read || !Get(read, layer + LayerNumber, output.number) || !Get(read, layer + LayerSheet, output.sheet))
		return LayerStatus::Unreadable;
	if (output.number != expected) return LayerStatus::Mismatch;
	for (size_t i = 0; i < TreeCount; ++i) {
		const uintptr_t header = layer + LayerTrees[i];
		if (!Get(read, header + TreeNodeCount, output.counts[i])) output.trees[i] = TreeStatus::Unreadable;
		else if (walk) output.trees[i] = ReadTree(read, header, static_cast<Tree>(i), limits, cells);
		else output.trees[i] = output.counts[i] > limits.maxNodes ? TreeStatus::TooLarge : TreeStatus::Ok;
	}
	return LayerStatus::Ok;
}

bool InRooms(const Cell& cell, const std::vector<NativeRooms::Room>& rooms) {
	const bool tiles = Tiles(cell.tree);
	if (tiles && !cell.lattice) return false;
	const int scale = tiles ? 1 : LevelExitLayout::SubtilesPerTile;
	for (const auto& room : rooms) {
		const int minX = room.tileX * scale, minY = room.tileY * scale;
		if (cell.x >= minX && cell.y >= minY && cell.x < minX + room.tileWidth * scale && cell.y < minY + room.tileHeight * scale)
			return true;
	}
	return false;
}

std::vector<NativeRooms::Room> RevealedRooms(const std::vector<Cell>& cells, const std::vector<NativeRooms::Room>& rooms) {
	std::unordered_set<uint64_t> tiles;
	for (const auto& cell : cells)
		if (Tiles(cell.tree) && cell.lattice) tiles.insert(TileKey(cell.x, cell.y));
	std::vector<NativeRooms::Room> result;
	for (const auto& room : rooms) {
		bool drawn = false;
		for (int y = room.tileY; !drawn && y < room.tileY + room.tileHeight; ++y)
			for (int x = room.tileX; !drawn && x < room.tileX + room.tileWidth; ++x) drawn = tiles.contains(TileKey(x, y));
		if (drawn) result.push_back(room);
	}
	return result;
}

std::vector<size_t> DrawnWalkable(const std::vector<Cell>& cells, const std::vector<std::vector<Point>>& rooms) {
	std::unordered_set<uint64_t> tiles;
	for (const auto& cell : cells)
		if (Tiles(cell.tree) && cell.lattice) tiles.insert(TileKey(cell.x, cell.y));
	std::vector<size_t> drawn;
	drawn.reserve(rooms.size());
	for (const auto& room : rooms)
		drawn.push_back(static_cast<size_t>(std::count_if(room.begin(), room.end(),
			[&](const Point& tile) { return tiles.contains(TileKey(tile.x, tile.y)); })));
	return drawn;
}

const char* TreeName(Tree tree) {
	switch (tree) {
	case Tree::Floor: return "floors";
	case Tree::Wall: return "walls";
	case Tree::Object: return "objects";
	case Tree::Extra: return "extras";
	}
	return "?";
}

const char* StatusName(TreeStatus status) {
	switch (status) {
	case TreeStatus::Ok: return "ok";
	case TreeStatus::Unreadable: return "unreadable";
	case TreeStatus::TooLarge: return "too-large";
	case TreeStatus::Corrupt: return "corrupt";
	}
	return "?";
}

}
