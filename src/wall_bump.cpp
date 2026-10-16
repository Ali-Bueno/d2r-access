#include "wall_bump.h"

#include <algorithm>

namespace D2RAccess::WallBump {
void Detector::Reset() { *this = Detector(settings_); }

bool Detector::Tick(const Sample& sample) {
	const bool broken = started_ && (sample.now < lastTick_ || sample.now - lastTick_ > settings_.maxGapMs
		|| sample.attempts < attempts_);
	if (!started_ || broken) {
		Reset();
		started_ = true;
		attempts_ = sample.attempts;
		previous_ = anchor_ = sample.position;
		lastTick_ = lastAttempt_ = sample.now;
		return false;
	}
	const bool attempted = sample.attempts > attempts_;
	// The game freezes the exact position of a refused move; sliding moves it.
	const bool still = sample.position.x == previous_.x && sample.position.y == previous_.y;
	attempts_ = sample.attempts;
	previous_ = sample.position;
	lastTick_ = sample.now;
	if (attempted) lastAttempt_ = sample.now;
	else if (sample.now - lastAttempt_ >= settings_.releaseMs) armed_ = true;
	if (std::hypot(sample.position.x - anchor_.x, sample.position.y - anchor_.y) >= settings_.moveDistance) {
		anchor_ = sample.position;
		armed_ = true;
	}
	const double length = std::hypot(sample.push.x, sample.push.y);
	if (!attempted || !still || !sample.contact || !(length > 0)) return false;
	const Radar::Position direction{sample.push.x / length, sample.push.y / length};
	if (!armed_ && direction.x * direction_.x + direction.y * direction_.y < settings_.turnCosine) armed_ = true;
	const bool again = !armed_ && sample.loopMs && sample.now - lastBump_ >= sample.loopMs;
	if ((!armed_ && !again) || (bumped_ && sample.now - lastBump_ < settings_.repeatMs)) return false;
	armed_ = false;
	bumped_ = true;
	direction_ = direction;
	anchor_ = sample.position;
	lastBump_ = sample.now;
	return true;
}

bool Contact(const Radar::World& world, Radar::Position push) {
	const double length = std::hypot(push.x, push.y);
	if (!(length > 0)) return false;
	const double ux = push.x / length, uy = push.y / length;
	// Screen up, down, left and right are world diagonals, whose wall can be
	// ContactCells away on an axis and so farther than that along the push.
	const double reach = ContactCells / std::max(std::abs(ux), std::abs(uy));
	const double x = world.player.x + world.subtileFraction.x, y = world.player.y + world.subtileFraction.y;
	// Off the map is a wall too: the game reads a cell outside every room as 0x27,
	// WALL included (docs/collision-semantics.md), the borders of the outdoor areas.
	const auto blocks = [](Radar::State state) { return state != Radar::State::Open; };
	for (double along = ContactStep; along <= reach; along += ContactStep) {
		const int cellX = static_cast<int>(std::floor(x + along * ux));
		const int cellY = static_cast<int>(std::floor(y + along * uy));
		if (blocks(world.Cell(cellX, cellY, Radar::Layer::Terrain))) return true;
		// The body one step on, a cell narrower: a ragged border can refuse it beside the line.
		if (along >= 1 && along <= reach - 1 && blocks(world.Body(cellX, cellY, Radar::Layer::Terrain))) return true;
	}
	return false;
}

double Pan(Radar::Position push) {
	const auto screen = Radar::ScreenOffset(push);
	const double length = std::hypot(screen.x, screen.y);
	return length > 0 ? screen.x / length : 0;
}
}
