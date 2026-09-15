#include "radar.h"

#include <cmath>
#include <numbers>

namespace D2RAccess::Radar {
namespace {
constexpr int Sectors = 8; // spec: movement quantized to eight headings
constexpr double SectorAngle = 2 * std::numbers::pi / Sectors;
int Sector(Position delta) {
	const int result = static_cast<int>(std::round(std::atan2(delta.y, delta.x) / SectorAngle));
	return (result + Sectors) % Sectors;
}
Position Axis(int sector) {
	return {std::cos(sector * SectorAngle), std::sin(sector * SectorAngle)};
}
// D2 world Y increases toward screen-left/down. Its isometric projection has
// positive determinant, so -90 degrees is the player's left (see radar docs).
constexpr std::array<int, DirectionCount> Offsets{0, -2, 2, 4};
}

void Engine::Reset() {
	const uint64_t nextRevision = revision_ + 1;
	*this = Engine(settings_);
	revision_ = nextRevision;
}

Events Engine::Tick(const World& world, uint64_t now) {
	Events events{};
	Position delta{world.player.x - previous_.x, world.player.y - previous_.y};
	double travel = std::hypot(delta.x, delta.y);
	if (!positioned_ || world.session != session_ || world.level != level_
		|| now < lastTick_ || now - lastTick_ > settings_.maxGapMs || travel >= settings_.teleportDistance) {
		Reset();
		positioned_ = true;
		session_ = world.session;
		level_ = world.level;
		travel = 0;
	}
	previous_ = world.player;
	lastTick_ = now;
	// Position getters report integer subtiles. Average displacement over three
	// 50 ms scans so an alternating X/Y staircase still has a diagonal heading.
	constexpr uint64_t HeadingWindowMs = 150;
	headingSamples_.push_back({world.player, now});
	while (headingSamples_.size() > 2 && now - headingSamples_.front().time > HeadingWindowMs) headingSamples_.pop_front();
	bool rebase = false;
	if (travel > 0) {
		const Position headingDelta{world.player.x - headingSamples_.front().position.x,
			world.player.y - headingSamples_.front().position.y};
		const int observedHeading = Sector(std::hypot(headingDelta.x, headingDelta.y) > 0 ? headingDelta : delta);
		if (observedHeading == heading_) {
			candidateHeading_ = -1;
			headingTravel_ = 0;
		} else {
			if (candidateHeading_ != observedHeading) {
				candidateHeading_ = observedHeading;
				headingSince_ = now;
				headingTravel_ = 0;
			}
			headingTravel_ += travel;
			if (headingTravel_ >= settings_.headingTravel && now - headingSince_ >= settings_.headingMs) {
				heading_ = candidateHeading_;
				candidateHeading_ = -1;
				rebase = true;
				++revision_;
			}
		}
	}
	if (heading_ < 0) return events; // no invented initial facing
	for (size_t i = 0; i < sensors_.size(); ++i) {
		auto& sensor = sensors_[i];
		sensor.probe = Scan(world, Axis(heading_ + Offsets[i]), settings_);
		const State observed = sensor.probe.observed;
		if (!sensor.probe.available) {
			sensor = Sensor{.probe = sensor.probe};
			continue;
		}
		if (rebase) {
			sensor = Sensor{.stable = observed, .probe = sensor.probe};
			continue;
		}
		// A pending turn is a change of reference frame, not topology. Discard
		// candidates so zig-zag cannot accumulate time or movement confirmation.
		if (observed == State::Unknown || observed == sensor.stable || candidateHeading_ >= 0) {
			sensor.candidate = State::Unknown;
			sensor.candidateTravel = 0;
			continue;
		}
		if (observed != sensor.candidate) {
			sensor.candidate = observed;
			sensor.candidateSince = now;
			sensor.candidateTravel = travel;
		} else sensor.candidateTravel += travel;
		if (now - sensor.candidateSince < settings_.stableMs) continue;
		if (sensor.stable != State::Unknown && sensor.candidateTravel < settings_.minTravel) continue;
		if (sensor.lastEmit && now - sensor.lastEmit < settings_.cooldownMs) continue;
		if (sensor.stable != State::Unknown) {
			events[i] = observed;
			sensor.lastEmit = now;
		}
		sensor.stable = observed;
		sensor.candidate = State::Unknown;
	}
	return events;
}
}
