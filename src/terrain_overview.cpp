#include "terrain_overview.h"
#include "spoken_direction.h"

#include "level_exit_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace D2RAccess::TerrainOverview {
namespace {
using LevelExitLayout::SubtilesPerTile;
// One step walks a tile along both world axes, which is exactly one screen
// cardinal, so it covers that tile diagonal (Radar::Diagonal is cos 45).
constexpr double StepSubtiles = SubtilesPerTile / Radar::Diagonal;
// Two map tiles: an unexplored stretch starting this close starts at the
// player's feet, and the distance adds nothing to the word "unexplored".
constexpr double NearStart = 2 * SubtilesPerTile;

size_t Index(Radar::Direction direction) { return static_cast<size_t>(direction); }
int Tile(double subtile) { return static_cast<int>(std::floor(subtile / SubtilesPerTile)); }
bool Inside(const TileRect& rect, int x, int y) {
	return x >= rect.x && y >= rect.y && x - rect.x < rect.width && y - rect.y < rect.height;
}
// Distance along a screen direction, over the radar's own axes, so a direction
// here can never end up mirrored against the rest of the mod.
double Along(Position point, Radar::Direction direction) {
	const auto& axis = Radar::Axes[Index(direction)];
	return point.x * axis.x + point.y * axis.y;
}
// The tile step of a screen direction: each screen cardinal is a world diagonal.
int StepX(Radar::Direction direction) { return Radar::Axes[Index(direction)].x < 0 ? -1 : 1; }
int StepY(Radar::Direction direction) { return Radar::Axes[Index(direction)].y < 0 ? -1 : 1; }

const Room* RoomAt(const Layout& layout, int x, int y) {
	for (const auto& room : layout.rooms)
		if (Inside(room.rect, x, y)) return &room;
	return nullptr;
}
std::string NeighbourAt(const Layout& layout, int x, int y) {
	for (const auto& neighbour : layout.neighbours)
		if (Inside(neighbour.rect, x, y)) return neighbour.name;
	return {};
}

// Which screen third of the level's span the player stands in, per axis.
size_t Place(const TileRect& box, Position player) {
	const double left = static_cast<double>(box.x) * SubtilesPerTile;
	const double top = static_cast<double>(box.y) * SubtilesPerTile;
	const double right = static_cast<double>(box.x + box.width) * SubtilesPerTile;
	const double bottom = static_cast<double>(box.y + box.height) * SubtilesPerTile;
	const Position corners[]{{left, top}, {right, top}, {left, bottom}, {right, bottom}};
	// The level's rectangle is a diamond on screen, so its span along a screen
	// axis is the span of its four corners.
	const auto third = [&](Radar::Direction direction) {
		double low = Along(corners[0], direction), high = low;
		for (const auto& corner : corners) {
			const double value = Along(corner, direction);
			low = std::min(low, value);
			high = std::max(high, value);
		}
		if (high <= low) return ThirdsPerAxis / 2;
		const int index = static_cast<int>((Along(player, direction) - low) / (high - low) * ThirdsPerAxis);
		return std::clamp(index, 0, ThirdsPerAxis - 1);
	};
	return static_cast<size_t>(third(Radar::Direction::Up) * ThirdsPerAxis + third(Radar::Direction::Right));
}

Leg Walk(const Layout& layout, const TileRect& box, int tileX, int tileY, Radar::Direction direction) {
	Leg leg;
	const int stepX = StepX(direction), stepY = StepY(direction);
	// A diagonal walk cannot stay inside the level longer than its two sides.
	const int maxSteps = box.width + box.height;
	bool walkedTerrain = false;
	for (int step = 1; step <= maxSteps; ++step) {
		const int x = tileX + stepX * step, y = tileY + stepY * step;
		leg.edge = step * StepSubtiles;
		const Room* room = RoomAt(layout, x, y);
		if (room == nullptr) { leg.border = NeighbourAt(layout, x, y); break; }
		walkedTerrain = true;
		if (!room->explored && !leg.hasUnexplored) {
			leg.hasUnexplored = true;
			leg.unexplored = leg.edge;
		}
	}
	leg.exploredToEdge = walkedTerrain && !leg.hasUnexplored;
	return leg;
}
}

TileRect Bounds(const std::vector<Room>& rooms) {
	TileRect box;
	bool first = true;
	for (const auto& room : rooms) {
		if (room.rect.width <= 0 || room.rect.height <= 0) continue;
		if (first) { box = room.rect; first = false; continue; }
		const int right = std::max(box.x + box.width, room.rect.x + room.rect.width);
		const int bottom = std::max(box.y + box.height, room.rect.y + room.rect.height);
		box.x = std::min(box.x, room.rect.x);
		box.y = std::min(box.y, room.rect.y);
		box.width = right - box.x;
		box.height = bottom - box.y;
	}
	return box;
}

Summary Describe(const Layout& layout, Position player) {
	Summary summary;
	const int tileX = Tile(player.x), tileY = Tile(player.y);
	if (RoomAt(layout, tileX, tileY) == nullptr) return summary;
	summary.box = Bounds(layout.rooms);
	summary.place = Place(summary.box, player);
	size_t explored = 0;
	for (const auto& room : layout.rooms) explored += room.explored ? 1 : 0;
	summary.percent = static_cast<int>(explored * 100 / layout.rooms.size());
	for (size_t d = 0; d < Radar::DirectionCount; ++d)
		summary.legs[d] = Walk(layout, summary.box, tileX, tileY, static_cast<Radar::Direction>(d));
	summary.valid = true;
	return summary;
}

std::string Speak(const Summary& summary, const std::string& levelName,
	const EntityRadar::Settings& scan, const Strings::Table& strings) {
	std::string directions;
	for (size_t d = 0; d < Radar::DirectionCount; ++d) {
		const Leg& leg = summary.legs[d];
		const std::string distance = EntityRadar::DistancePhrase(leg.edge, scan, strings);
		char reach[192]{};
		if (leg.border.empty()) std::snprintf(reach, sizeof reach, strings.terrainEdge, distance.c_str());
		else std::snprintf(reach, sizeof reach, strings.terrainBorder, leg.border.c_str(), distance.c_str());
		std::string state;
		if (leg.exploredToEdge) state = strings.terrainExplored;
		else if (leg.hasUnexplored && leg.unexplored >= NearStart) {
			char text[96]{};
			std::snprintf(text, sizeof text, strings.terrainUnexploredFrom,
				EntityRadar::DistancePhrase(leg.unexplored, scan, strings).c_str());
			state = text;
		} else if (leg.hasUnexplored) state = strings.terrainUnexplored;
		if (!state.empty()) state.insert(0, strings.listSeparator);
		char line[320]{};
		std::snprintf(line, sizeof line, strings.terrainDirection, SpokenDirection::OfSide(static_cast<Radar::Direction>(d), strings),
			reach, state.c_str());
		if (!directions.empty()) directions += strings.terrainSeparator;
		directions += line;
	}
	// The place and the directions are separated the same way the directions are.
	directions.insert(0, strings.terrainSeparator);
	char percent[96]{};
	std::snprintf(percent, sizeof percent, strings.terrainPercent, summary.percent);
	std::string place = strings.terrainPlaces[std::min(summary.place, Strings::TerrainPlaceCount - 1)];
	place += strings.listSeparator;
	place += percent;
	char text[1024]{};
	std::snprintf(text, sizeof text, strings.terrainOverview, levelName.c_str(), place.c_str(), directions.c_str());
	return text;
}
}
