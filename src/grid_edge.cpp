#include "grid_edge.h"

namespace D2RAccess::GridEdge {
namespace {
bool AtEdge(const Sample& s) {
	return (s.dx < 0 && s.x == 0) || (s.dx > 0 && s.width > 0 && s.x == s.width - 1)
		|| (s.dy < 0 && s.y == 0) || (s.dy > 0 && s.height > 0 && s.y == s.height - 1);
}
}

std::optional<std::pair<int, int>> Detector::Update(const Sample& sample) {
	if (!sample.grid || sample.x < 0 || (!sample.dx && !sample.dy)) {
		Reset();
		return std::nullopt;
	}
	if (!pushing_ || sample.grid != anchor_.grid || sample.dx != anchor_.dx || sample.dy != anchor_.dy) {
		anchor_ = sample;
		pushing_ = true;
		moved_ = sounded_ = false;
		return std::nullopt;
	}
	if (sample.x != anchor_.x || sample.y != anchor_.y) {
		anchor_ = sample;
		moved_ = true;
		sounded_ = false;
		return std::nullopt;
	}
	const uint64_t still = sample.now >= anchor_.now ? sample.now - anchor_.now : 0;
	if (sounded_ || (moved_ ? !AtEdge(sample) || still < HeldSettleMs : still < SettleMs)) return std::nullopt;
	sounded_ = true;
	return std::pair{sample.dx, sample.dy};
}
}
