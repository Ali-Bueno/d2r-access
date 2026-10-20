#include "step_counter.h"

#include <cmath>

namespace D2RAccess::StepCounter {
int Odometer::Walk(GroundItems::Point position, uint64_t session, double every) {
	if (!std::isfinite(position.x) || !std::isfinite(position.y) || !(every > 0)) { Reset(); return 0; }
	if (!last_ || session != session_) {
		last_ = position;
		session_ = session;
		walked_ = 0;
		return 0;
	}
	const double step = std::hypot(position.x - last_->x, position.y - last_->y);
	last_ = position;
	if (step > JumpUnits) return 0;
	walked_ += step;
	const int clicks = static_cast<int>(walked_ / every);
	walked_ -= clicks * every;
	return clicks;
}

void Odometer::Reset() {
	last_.reset();
	walked_ = 0;
}
}
