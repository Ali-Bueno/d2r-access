#include "target_sight.h"

#include <cctype>
#include <cmath>

namespace D2RAccess::TargetSight {
namespace {
bool Boundary(unsigned char c) { return c < 128 && !std::isalnum(c); }
}

bool Names(std::string_view label, std::string_view name) {
	if (name.empty() || label.empty()) return false;
	for (size_t pos = label.find(name); pos != std::string_view::npos; pos = label.find(name, pos + 1)) {
		const bool left = pos == 0 || Boundary(static_cast<unsigned char>(label[pos - 1]));
		const size_t after = pos + name.size();
		const bool right = after == label.size() || Boundary(static_cast<unsigned char>(label[after]));
		if (left && right) return true;
	}
	return false;
}

Sight Of(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	std::string_view label, const Settings& settings) {
	if (!settings.enabled || label.empty()) return Sight::Unknown;
	bool bearer = false;
	for (const auto& entity : entities) {
		// Items are announced by their own proximity service, which decides on
		// distance rather than sight; a level exit is a fixed landmark the scan
		// itself exempts from walls.
		if (entity.kind == EntityRadar::Kind::Item || entity.kind == EntityRadar::Kind::Exit) continue;
		if (!Names(label, entity.worldName.empty() ? entity.name : entity.worldName)) continue;
		const double distance = std::hypot(entity.position.x - world.player.x, entity.position.y - world.player.y);
		if (!std::isfinite(distance) || distance > settings.reach) continue;
		bearer = true;
		if (EntityRadar::LineOfSight(world, world.player, entity)) return Sight::Visible;
	}
	// Every bearer of that name within reach is behind a wall: the game is
	// naming something the player cannot see.
	return bearer ? Sight::Hidden : Sight::Unknown;
}
}
