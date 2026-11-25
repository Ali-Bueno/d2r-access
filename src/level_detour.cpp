#include "level_detour.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::LevelRoute {
Detours::Detours(const Memory& here, const Memory& there, Goal goal, Position from, int bodyPattern, bool guess)
	: goalHere_(goal == Goal::Here) {
	if (!joined_.Join(here, there, goalHere_ || guess, !goalHere_ || guess)) return;
	foreign_.assign(joined_.Cells().size(), 0);
	for (int y = 0; y < joined_.Height(); ++y)
		for (int x = 0; x < joined_.Width(); ++x) {
			const int atX = joined_.X() + x, atY = joined_.Y() + y;
			foreign_[static_cast<size_t>(y) * joined_.Width() + x] =
				here.At(atX, atY) == Ground::Outside && there.At(atX, atY) != Ground::Outside;
		}
	// A way is the same both ways, so one search from the player serves every room.
	// The cell stood on need not fit the body (Field::From): those beside it start too.
	const int x = static_cast<int>(std::lround(from.x)), y = static_cast<int>(std::lround(from.y));
	field_.emplace(joined_, Area{x - 1, y - 1, x + 1, y + 1}, bodyPattern, true, true);
}

bool Detours::Foreign(Position at) const {
	const int x = static_cast<int>(std::lround(at.x)) - joined_.X(), y = static_cast<int>(std::lround(at.y)) - joined_.Y();
	return foreign_[static_cast<size_t>(y) * joined_.Width() + x] != 0;
}

std::optional<Detour> Detours::To(const Area& room) const {
	if (!field_) return std::nullopt;
	const auto nearest = field_->Nearest(room);
	if (!nearest) return std::nullopt;
	auto path = field_->From(*nearest);
	if (path.empty()) return std::nullopt;
	std::reverse(path.begin(), path.end());
	path.push_back(*nearest);
	Detour detour;
	detour.steps = static_cast<int>(path.size()) - 1;
	if (goalHere_) {
		const auto left = std::find_if(path.begin(), path.end(), [&](Position at) { return Foreign(at); });
		if (left == path.end() || left == path.begin()) return std::nullopt; // no neighbour on the way
		detour.leave = *(left - 1);
	}
	const auto last = std::find_if(path.rbegin(), path.rend(), [&](Position at) { return Foreign(at) == goalHere_; });
	if (last == path.rend()) return std::nullopt;
	detour.back = *last;
	return detour;
}
}
