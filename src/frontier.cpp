#include "frontier.h"
#include "navigation_grid.h"

#include <cmath>

namespace D2RAccess::Frontier {
namespace {
// Native movement trace 0x364700 allows diagonal destination steps.
constexpr int Steps[8][2] = {{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}};
bool Inside(const std::vector<NativeRooms::Bounds>& unseen, int x, int y) {
	for (const auto& bounds : unseen) if (NativeRooms::Contains(bounds, x, y)) return true;
	return false;
}
}

Radar::Direction Quadrant(double dx, double dy) {
	// Native projection 0x334DB0: screen X grows with (X-Y), screen Y with (X+Y).
	const double right = dx - dy, down = dx + dy;
	if (std::abs(right) >= std::abs(down)) return right < 0 ? Radar::Direction::Left : Radar::Direction::Right;
	return down < 0 ? Radar::Direction::Up : Radar::Direction::Down;
}

Radar::Marks Quadrants(const Radar::World& world, const std::vector<NativeRooms::Bounds>& unseen) {
	Radar::Marks marks{};
	if (unseen.empty()) return marks;
	const auto grid = Navigation::Build(world, Navigation::MaxRadius);
	if (!grid.originKnown) return marks;
	for (int y = grid.y; y < grid.y + grid.height; ++y) {
		for (int x = grid.x; x < grid.x + grid.width; ++x) {
			const auto* cell = grid.At(x, y);
			if (!cell || cell->steps < 0) continue; // not reachable from the player
			for (const auto& step : Steps) {
				const int nx = x + step[0], ny = y + step[1];
				const auto* next = grid.At(nx, ny);
				if (next && next->body != Radar::State::Unknown) continue; // loaded ground, not the edge
				if (!Inside(unseen, nx, ny)) continue;
				marks[static_cast<size_t>(Quadrant(nx - world.player.x, ny - world.player.y))] = true;
				break;
			}
		}
	}
	return marks;
}
}
