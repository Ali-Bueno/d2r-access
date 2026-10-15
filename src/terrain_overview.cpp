#include "terrain_overview.h"
#include "spoken_direction.h"

#include "level_exit_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace D2RAccess::TerrainOverview {
namespace {
using LevelExitLayout::SubtilesPerTile;
// Two map tiles: an unexplored stretch starting this close starts at the
// player's feet, and the distance adds nothing to the word "unexplored".
constexpr double NearStart = 2 * SubtilesPerTile;

size_t Index(Side side) { return static_cast<size_t>(side); }
int Tile(double subtile) { return static_cast<int>(std::floor(subtile / SubtilesPerTile)); }
bool Inside(const TileRect& rect, int x, int y) {
	return x >= rect.x && y >= rect.y && x - rect.x < rect.width && y - rect.y < rect.height;
}
// The tile step towards a side: one world axis, the other untouched.
int StepX(Side side) { return static_cast<int>(SideSteps[Index(side)].x); }
int StepY(Side side) { return static_cast<int>(SideSteps[Index(side)].y); }

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

// Which third of the level's span the player stands in along each world axis.
// Thirds along x run from the northwest side to the southeast one, along y from
// the northeast side to the southwest one; the index is x's third times three
// plus y's, the order of Strings::Table::terrainPlaces.
size_t Place(const TileRect& box, Position player) {
	const auto third = [](double low, double span, double value) {
		if (span <= 0) return ThirdsPerAxis / 2;
		const int index = static_cast<int>((value - low) / span * ThirdsPerAxis);
		return std::clamp(index, 0, ThirdsPerAxis - 1);
	};
	const double left = static_cast<double>(box.x) * SubtilesPerTile, top = static_cast<double>(box.y) * SubtilesPerTile;
	const int x = third(left, static_cast<double>(box.width) * SubtilesPerTile, player.x);
	const int y = third(top, static_cast<double>(box.height) * SubtilesPerTile, player.y);
	return static_cast<size_t>(x * ThirdsPerAxis + y);
}

Leg Walk(const Layout& layout, const TileRect& box, int tileX, int tileY, Side side) {
	Leg leg;
	const int stepX = StepX(side), stepY = StepY(side);
	// A walk along one axis cannot stay inside the level longer than that side.
	const int maxSteps = std::max(box.width, box.height) + 1;
	bool walkedTerrain = false;
	for (int step = 1; step <= maxSteps; ++step) {
		const int x = tileX + stepX * step, y = tileY + stepY * step;
		leg.edge = step * SubtilesPerTile;
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
	for (size_t d = 0; d < SideCount; ++d)
		summary.legs[d] = Walk(layout, summary.box, tileX, tileY, static_cast<Side>(d));
	summary.valid = true;
	return summary;
}

std::string Speak(const Summary& summary, const std::string& levelName,
	const EntityRadar::Settings& scan, const Strings::Table& strings, std::vector<Landmark> landmarks) {
	std::string directions;
	for (size_t d = 0; d < SideCount; ++d) {
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
		std::snprintf(line, sizeof line, strings.terrainDirection, SpokenDirection::Of(SideSteps[d], strings), reach, state.c_str());
		if (!directions.empty()) directions += strings.terrainSeparator;
		directions += line;
	}
	// The places found, nearest first, as the scanner says where a thing stands.
	std::sort(landmarks.begin(), landmarks.end(), [](const Landmark& a, const Landmark& b) {
		return std::hypot(a.offset.x, a.offset.y) < std::hypot(b.offset.x, b.offset.y);
	});
	for (const auto& landmark : landmarks) {
		if (landmark.name.empty()) continue;
		const double distance = std::hypot(landmark.offset.x, landmark.offset.y);
		std::string line = landmark.name + strings.listSeparator + EntityRadar::DistancePhrase(distance, scan, strings);
		if (!EntityRadar::UnderPlayer(distance, scan)) {
			line += strings.listSeparator;
			line += SpokenDirection::Of(landmark.offset, strings);
		}
		directions += strings.terrainSeparator;
		directions += line;
	}
	// The place and the directions are separated the same way the directions are.
	directions.insert(0, strings.terrainSeparator);
	char percent[96]{};
	std::snprintf(percent, sizeof percent, strings.terrainPercent, summary.percent);
	std::string place = strings.terrainPlaces[std::min(summary.place, Strings::TerrainPlaceCount - 1)];
	place += strings.listSeparator;
	place += percent;
	char text[1600]{};
	std::snprintf(text, sizeof text, strings.terrainOverview, levelName.c_str(), place.c_str(), directions.c_str());
	return text;
}
}
