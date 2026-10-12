#pragma once

#include "native_rooms.h"
#include "radar.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

// A level of Across x Down rooms whose first tile is not the map's origin, and
// what the game would have loaded of it round a player: the level-route and
// beacon-guidance tests' shared fixture.
namespace LevelFixture {
constexpr uint16_t PlayerPath = 0x1C09; // observed native player walking mask
constexpr int RoomTiles = 8, RoomCells = RoomTiles * 5;
constexpr int Across = 5, Down = 5, TileX = 100, TileY = 100;
constexpr int Left = TileX * 5, Top = TileY * 5, Width = Across * RoomCells, Height = Down * RoomCells;

struct Level {
	std::vector<uint16_t> cells = std::vector<uint16_t>(static_cast<size_t>(Width) * Height, 0);
	uint16_t& At(int x, int y) { return cells[static_cast<size_t>(y - Top) * Width + (x - Left)]; }
	void Fill(int x0, int y0, int x1, int y1, uint16_t flags) { // inclusive, level cells
		for (int y = y0; y <= y1; ++y)
			for (int x = x0; x <= x1; ++x) At(x, y) = flags;
	}
	std::vector<D2RAccess::NativeRooms::Room> Rooms() const {
		std::vector<D2RAccess::NativeRooms::Room> rooms;
		for (int row = 0; row < Down; ++row)
			for (int column = 0; column < Across; ++column) {
				const int x = TileX + column * RoomTiles, y = TileY + row * RoomTiles;
				rooms.push_back({x, y, RoomTiles, RoomTiles, D2RAccess::NativeRooms::Identity(x, y)});
			}
		return rooms;
	}
	// What the game would have loaded: the rooms within `reach` rooms of the player's.
	D2RAccess::Radar::World Loaded(D2RAccess::Radar::Position player, int reach, int pattern = 1) const {
		D2RAccess::Radar::World world;
		world.session = world.level = 1;
		world.player = player;
		world.bodyPattern = pattern;
		world.walkingMask = PlayerPath;
		const int column = (static_cast<int>(player.x) - Left) / RoomCells, row = (static_cast<int>(player.y) - Top) / RoomCells;
		for (int r = std::max(0, row - reach); r <= std::min(Down - 1, row + reach); ++r)
			for (int c = std::max(0, column - reach); c <= std::min(Across - 1, column + reach); ++c) {
				D2RAccess::Radar::RoomGrid room{Left + c * RoomCells, Top + r * RoomCells, RoomCells, RoomCells, {}};
				for (int y = 0; y < RoomCells; ++y)
					for (int x = 0; x < RoomCells; ++x)
						room.cells.push_back(cells[static_cast<size_t>(room.y - Top + y) * Width + (room.x - Left + x)]);
				world.rooms.push_back(std::move(room));
			}
		return world;
	}
};
}
