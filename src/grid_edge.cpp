#include "grid_edge.h"

namespace D2RAccess::GridEdge {
namespace {
bool AtEdge(const Sample& s) {
	return (s.dx < 0 && s.x == 0) || (s.dx > 0 && s.width > 0 && s.x == s.width - 1)
		|| (s.dy < 0 && s.y == 0) || (s.dy > 0 && s.height > 0 && s.y == s.height - 1);
}
}

// A press that began on the edge cell is judged once, whether or not the button is still down.
Outcome Detector::Judge(const Sample& sample) {
	Outcome outcome{Outcome::Kind::None, anchor_.dx, anchor_.dy, anchor_.x, anchor_.y};
	if (!sample.grid || sample.grid != anchor_.grid || sample.x < 0) outcome.kind = Outcome::Kind::Left;
	else if (sample.x != anchor_.x || sample.y != anchor_.y) { pending_ = false; return outcome; }
	else if (sample.now >= anchor_.now + SettleMs) outcome.kind = Outcome::Kind::Knock;
	else return outcome;
	pending_ = false;
	sounded_ = outcome.kind == Outcome::Kind::Knock;
	return outcome;
}

Outcome Detector::Update(const Sample& sample) {
	const Outcome outcome = pending_ ? Judge(sample) : Outcome{};
	if (!sample.grid || sample.x < 0 || (!sample.dx && !sample.dy)) {
		pushing_ = moved_ = sounded_ = false;
		return outcome;
	}
	if (!pushing_ || sample.grid != anchor_.grid || sample.dx != anchor_.dx || sample.dy != anchor_.dy) {
		anchor_ = sample;
		pushing_ = true;
		pending_ = AtEdge(sample);
		moved_ = sounded_ = false;
		return outcome;
	}
	if (sample.x != anchor_.x || sample.y != anchor_.y) {
		anchor_ = sample;
		moved_ = true;
		sounded_ = false;
		return outcome;
	}
	const uint64_t still = sample.now >= anchor_.now ? sample.now - anchor_.now : 0;
	if (outcome.kind != Outcome::Kind::None || sounded_ || !moved_ || !AtEdge(sample) || still < HeldSettleMs) return outcome;
	sounded_ = true;
	return {Outcome::Kind::Knock, sample.dx, sample.dy, sample.x, sample.y};
}

Crossing CrossingTracker::Update(uintptr_t panel, int dx, int dy, uint64_t now) {
	if (dx || dy) {
		dx_ = dx;
		dy_ = dy;
		pushAt_ = now;
		pushed_ = true;
	}
	if (!panel) return {};
	Crossing result;
	const bool remembered = panel_ && now - panelAt_ <= PanelMemoryMs;
	const bool pushedRecently = pushed_ && now - pushAt_ <= PushWindowMs;
	const bool cooled = !crossedBefore_ || now - crossedAt_ >= CrossingCooldownMs;
	if (remembered && panel != panel_ && pushedRecently && cooled) {
		result = {true, dx_, dy_, panel_, panel};
		crossedAt_ = now;
		crossedBefore_ = true;
	}
	panel_ = panel;
	panelAt_ = now;
	return result;
}
}
