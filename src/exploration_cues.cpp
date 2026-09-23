#include "exploration_cues.h"

#include "ground_items.h"
#include "level_exit_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <tuple>

namespace D2RAccess::ExplorationCues {
namespace {
using NativeLevelExits::TileRect;
// NativeLevelExits::ReadRect uses this bound before converting tile rectangles to 16-bit subtiles.
constexpr int MaxTile = UINT16_MAX / LevelExitLayout::SubtilesPerTile;

bool Valid(const TileRect& rect) {
	return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0
		&& rect.width <= MaxTile && rect.height <= MaxTile
		&& rect.x <= MaxTile - rect.width && rect.y <= MaxTile - rect.height;
}

bool Contains(const TileRect& rect, Radar::Position point) {
	const double scale = LevelExitLayout::SubtilesPerTile;
	return point.x >= rect.x * scale && point.y >= rect.y * scale
		&& point.x < (rect.x + rect.width) * scale
		&& point.y < (rect.y + rect.height) * scale;
}

Radar::Position NearestPoint(const TileRect& rect, Radar::Position player) {
	const double scale = LevelExitLayout::SubtilesPerTile;
	return {
		std::clamp(player.x, rect.x * scale, (rect.x + rect.width) * scale - 1),
		std::clamp(player.y, rect.y * scale, (rect.y + rect.height) * scale - 1)
	};
}

auto Key(const TileRect& rect) {
	return std::tuple{rect.x, rect.y, rect.width, rect.height};
}
}

Selection Select(const NativeLevelExits::Layout& layout, Radar::Position player,
	const ExploredRooms::Memory& memory, bool maze) {
	Selection result;
	if (!maze || !layout.level || !Valid(layout.rect)
		|| !std::isfinite(player.x) || !std::isfinite(player.y)) return result;

	const NativeLevelExits::LevelRoom* current = nullptr;
	for (const auto& room : layout.rooms) {
		if (!Valid(room.rect)) return {};
		if (room.active && Contains(room.rect, player)
			&& (!current || Key(room.rect) < Key(current->rect))) current = &room;
	}
	if (!current) return result;
	if (!memory.Visited(layout.level, current->rect.x, current->rect.y))
		result.entered = Room{current->rect, player};

	double nearestDistanceSquared = GroundItems::AudibleRadius * GroundItems::AudibleRadius;
	for (const auto& room : layout.rooms) {
		if (!room.active || &room == current
			|| memory.Visited(layout.level, room.rect.x, room.rect.y)) continue;
		const auto point = NearestPoint(room.rect, player);
		const double dx = point.x - player.x, dy = point.y - player.y;
		const double distanceSquared = dx * dx + dy * dy;
		if (distanceSquared < nearestDistanceSquared
			|| (result.nearest && distanceSquared == nearestDistanceSquared
				&& Key(room.rect) < Key(result.nearest->rect))) {
			nearestDistanceSquared = distanceSquared;
			result.nearest = Room{room.rect, point};
		}
	}
	return result;
}
}
