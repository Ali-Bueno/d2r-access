#include "radar.h"

#include <cmath>
#include <numbers>

namespace D2RAccess::Radar {
namespace {
// Native projection 0x334DB0: screen X = 80*(X-Y), screen Y = 40*(X+Y).
// Stick up/left/right/down map to these fixed world diagonals.
constexpr double Diagonal = std::numbers::sqrt2 / 2;
constexpr std::array<Position, DirectionCount> Axes{{{-Diagonal, -Diagonal},
	{-Diagonal, Diagonal}, {Diagonal, -Diagonal}, {Diagonal, Diagonal}}};
}
void Engine::Reset() {
	const uint64_t nextRevision = revision_ + 1;
	*this = Engine(settings_);
	revision_ = nextRevision;
}
Events Engine::Tick(const World& world, uint64_t now) {
	Events events{};
	double travel = std::hypot(world.player.x - previous_.x, world.player.y - previous_.y);
	const bool rebase = !positioned_ || world.session != session_ || world.level != level_
		|| now < lastTick_ || now - lastTick_ > settings_.maxGapMs || travel >= settings_.teleportDistance;
	if (rebase) {
		Reset();
		positioned_ = true;
		session_ = world.session;
		level_ = world.level;
		travel = 0;
	}
	previous_ = world.player;
	lastTick_ = now;
	for (size_t i = 0; i < sensors_.size(); ++i) {
		auto& sensor = sensors_[i];
		sensor.probe = Scan(world, Axes[i], settings_);
		const State observed = sensor.probe.observed;
		if (!sensor.probe.available) {
			sensor = Sensor{.probe = sensor.probe};
			continue;
		}
		if (rebase) {
			sensor = Sensor{.stable = observed, .probe = sensor.probe};
			continue;
		}
		if (observed == State::Unknown || observed == sensor.stable) {
			sensor.candidate = State::Unknown;
			sensor.candidateTravel = 0;
			continue;
		}
		if (observed != sensor.candidate) {
			sensor.candidate = observed;
			sensor.candidateSince = now;
			sensor.candidateTravel = travel;
		} else sensor.candidateTravel += travel;
		const uint64_t age = now - sensor.candidateSince;
		if (age < settings_.stableMs) continue;
		// Stopping against an obstacle must not suppress its closing cue.
		// A longer stationary window also confirms doors while rejecting flicker.
		if (sensor.stable != State::Unknown && sensor.candidateTravel < settings_.minTravel && age < settings_.stationaryMs) continue;
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
