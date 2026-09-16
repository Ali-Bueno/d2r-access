#include "radar.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::Radar {
namespace {
// D2 collision definitions, corroborated by the native reader; see
// docs/radar-object-filtering.md. Other movement restrictions stay structural.
constexpr uint16_t Wall = 0x0001, Blank = 0x0020, Object = 0x0400;
// Five collision subtiles form one native map tile. This is a conservative
// size policy for baked clutter sharing WALL, not an object type identifier.
constexpr int SubtilesPerTile = 5;
int ObjectSpan(const Settings& settings) { return std::max(SubtilesPerTile, static_cast<int>(std::ceil(settings.boundaryRange))); }
struct Bounds { int left, top, right, bottom; };

bool CanBypass(const World& original, Bounds obstacle) {
	// Inspect destination centres one cell beyond the expanded body footprint.
	// The original map proves the detour: previously removed clutter cannot
	// make another object appear bypassable. Creatures do not change geometry.
	const int margin = (original.bodyPattern == 0 ? 0 : 1) + 1;
	const int left = obstacle.left - margin, top = obstacle.top - margin;
	const int width = obstacle.right - obstacle.left + 1 + 2*margin;
	const int height = obstacle.bottom - obstacle.top + 1 + 2*margin;
	std::vector<State> cells(static_cast<size_t>(width)*height);
	size_t entrance = cells.size();
	for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
		const size_t index = static_cast<size_t>(y)*width+x;
		cells[index] = original.Body(left+x, top+y);
		if (cells[index] == State::Unknown) return false;
		if ((x == 0 || y == 0 || x == width-1 || y == height-1) && cells[index] == State::Open)
			entrance = index;
	}
	if (entrance == cells.size()) return false;
	std::vector<size_t> reached{entrance};
	cells[entrance] = State::Blocked;
	for (size_t cursor = 0; cursor < reached.size(); ++cursor) {
		const int x = static_cast<int>(reached[cursor] % width), y = static_cast<int>(reached[cursor] / width);
		// Native 0x364700 permits diagonal destination steps. Every destination
		// here already passed the full native body pattern, not just a point.
		for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
			const int nx = x+dx, ny = y+dy;
			if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
			const size_t next = static_cast<size_t>(ny)*width+nx;
			if (cells[next] != State::Open) continue;
			cells[next] = State::Blocked;
			reached.push_back(next);
		}
	}
	// Preserve disconnected approaches AND enclosed walkable pockets. Checking
	// only the perimeter would erase a small room whose exterior is bypassable.
	return std::none_of(cells.begin(), cells.end(), [](State cell) { return cell == State::Open; });
}
}

int StructuralRadius(const Settings& settings, int bodyPattern) {
	const int bodyRadius = bodyPattern == 0 ? 0 : 1;
	return static_cast<int>(std::ceil(settings.boundaryRange)) + ObjectSpan(settings) + 2*bodyRadius + 1;
}
World StructuralWorld(const World& world, const Settings& settings) {
	World result;
	result.player = world.player; result.subtileFraction = world.subtileFraction;
	result.session = world.session; result.level = world.level;
	result.bodyPattern = world.bodyPattern; result.walkingMask = world.walkingMask;
	// The halo contains an entire eligible component, its detour and the body
	// checks beyond that detour, even when the hit is at the radar range edge.
	const int objectSpan = ObjectSpan(settings);
	const int radius = StructuralRadius(settings,world.bodyPattern);
	const int side = radius*2+1;
	result.rooms.push_back({static_cast<int>(world.player.x)-radius, static_cast<int>(world.player.y)-radius,
		side, side, std::vector<uint16_t>(static_cast<size_t>(side)*side)});
	auto& grid = result.rooms.front();
	for (int y = 0; y < side; ++y) for (int x = 0; x < side; ++x) {
		uint16_t flags = 0;
		const auto state = world.Cell(grid.x+x, grid.y+y, Layer::Terrain, &flags);
		grid.cells[static_cast<size_t>(y)*side+x] = state == State::Unknown ? Blank : TerrainCollisionFlags(flags);
	}
	const World original = result;
	std::vector<bool> visited(grid.cells.size());
	std::vector<size_t> component;
	for (size_t start = 0; start < grid.cells.size(); ++start) {
		if (visited[start] || (original.rooms[0].cells[start] & Blank)
			|| !(original.rooms[0].cells[start] & world.walkingMask)) continue;
		component.assign(1, start); visited[start] = true;
		const int sx = static_cast<int>(start % side), sy = static_cast<int>(start / side);
		Bounds bounds{sx, sy, sx, sy};
		uint16_t restrictions = 0;
		bool complete = true;
		for (size_t cursor = 0; cursor < component.size(); ++cursor) {
			const size_t index = component[cursor];
			const int x = static_cast<int>(index % side), y = static_cast<int>(index / side);
			bounds.left = std::min(bounds.left, x); bounds.right = std::max(bounds.right, x);
			bounds.top = std::min(bounds.top, y); bounds.bottom = std::max(bounds.bottom, y);
			restrictions |= original.rooms[0].cells[index] & world.walkingMask;
			// Eight-connected components preserve diagonal walls as a unit.
			for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
				const int nx = x+dx, ny = y+dy;
				if (nx < 0 || ny < 0 || nx >= side || ny >= side) { complete = false; continue; }
				const size_t next = static_cast<size_t>(ny)*side+nx;
				const uint16_t flags = original.rooms[0].cells[next];
				if (flags & Blank) { complete = false; continue; }
				if (visited[next] || !(flags & world.walkingMask)) continue;
				visited[next] = true; component.push_back(next);
			}
		}
		const int span = restrictions == Object ? objectSpan : SubtilesPerTile;
		if (!complete || (restrictions & ~(Wall | Object)) || bounds.right-bounds.left+1 > span
			|| bounds.bottom-bounds.top+1 > span) continue;
		if (!CanBypass(original, {grid.x+bounds.left, grid.y+bounds.top, grid.x+bounds.right, grid.y+bounds.bottom})) continue;
		for (const size_t index : component) grid.cells[index] &= ~world.walkingMask;
	}
	return result;
}
}
