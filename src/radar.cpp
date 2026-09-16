#include "radar.h"
#include "radar_passage.h"

#include <algorithm>
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
void Engine::UpdateMotion(Position player, uint64_t now) {
	if (!motion_.empty() && (player.x != motion_.back().position.x || player.y != motion_.back().position.y)) lastMove_ = now;
	if (!motion_.empty() && motion_.back().time == now) motion_.back().position = player;
	else motion_.push_back({player, now});
	while (motion_.size() > 1 && now - motion_.front().time > settings_.motionWindowMs) motion_.pop_front();
	velocity_ = {};
	const auto& first = motion_.front();
	if (now == first.time || now - lastMove_ >= settings_.stoppedMs) return;
	const double seconds = (now - first.time) / 1000.0;
	velocity_ = {(player.x - first.position.x) / seconds, (player.y - first.position.y) / seconds};
	// Use recent motion to describe acceleration and turns without lingering
	// on the previous direction. Motion does not generate geometry cues.
	const auto recent = std::find_if(motion_.begin(), motion_.end(), [&](const auto& sample) { return now - sample.time <= settings_.recentMotionMs; });
	if (recent == motion_.end() || recent->time == now) return;
	const double recentSeconds = (now - recent->time) / 1000.0;
	const Position delta{player.x - recent->position.x, player.y - recent->position.y};
	const double distance = std::hypot(delta.x, delta.y);
	if (distance == 0) return;
	const double speed = std::max(std::hypot(velocity_.x, velocity_.y), distance / recentSeconds);
	velocity_ = {delta.x * speed / distance, delta.y * speed / distance};
}
Events Engine::Tick(const World& world, uint64_t now, AudioLoad audio) {
	Events events{};
	// Preserve subcell movement for the actual-motion readout. The
	// collision adapter still supplies the native integer footprint origin.
	const Position motion{world.player.x + world.subtileFraction.x, world.player.y + world.subtileFraction.y};
	const double travel = std::hypot(motion.x - previous_.x, motion.y - previous_.y);
	const bool rebase = !positioned_ || world.session != session_ || world.level != level_
		|| now < lastTick_ || now - lastTick_ > settings_.maxGapMs || travel >= settings_.teleportDistance;
	if (rebase) {
		Reset();
		positioned_ = true;
		session_ = world.session;
		level_ = world.level;
	}
	previous_ = motion;
	lastTick_ = now;
	UpdateMotion(motion, now);
	if (rebase) audio = {}; // the service cancels old-world audio after this tick
	const auto structure = StructuralWorld(world, settings_);
	const int radius = PassageRadius(settings_, world.bodyPattern);
	const auto structuralGrid = Navigation::Build(structure, radius);
	const auto movementGrid = Navigation::Build(world, radius);
	for (size_t i = 0; i < sensors_.size(); ++i) {
		auto& sensor = sensors_[i];
		sensor.probe = Scan(world, Axes[i], settings_);
		sensor.terrain = Scan(world, Axes[i], settings_, Layer::Terrain);
		sensor.structure = Scan(structure, Axes[i], settings_, Layer::Terrain);
		sensor.passage = ScanPassage(structuralGrid, Axes[i], settings_);
		sensor.movementPassage = ScanPassage(movementGrid, Axes[i], settings_);
		// Sideways steps along an angled wall add route length without
		// extending the useful turn. Keep the direct near-wall warning when
		// no connected route reaches the opening depth.
		if (sensor.passage.available && sensor.passage.depth < settings_.openDepth
			&& sensor.structure.end == State::Blocked && sensor.structure.clearDepth <= sensor.passage.closeDepth)
			sensor.passage.observed = State::Blocked;
		// Connected but offset travel can retain an existing opening. A new
		// opening requires alignment with the direct structural passage.
		if (sensor.passage.observed == State::Open && sensor.structure.observed != State::Open)
			sensor.passage.observed = State::Unknown;
		sensor.guidance = EvaluateGuidance(world, Axes[i], sensor.terrain, sensor.probe, velocity_, settings_);
		// A nearby offset route retains the passage state, but the opening
		// cue waits until the actual named direction is clear for the body.
		sensor.guidance.openingSafe &= sensor.movementPassage.observed == State::Open;
		sensor.guidance.warningDepth = sensor.passage.closeDepth;
		if (sensor.guidance.observed != State::Unknown) sensor.stable = sensor.guidance.observed;
		else if (!sensor.probe.available) sensor.stable = State::Unknown;
		sensor.newOpening = sensor.wallMemory.Update(sensor.structure, world.player, Axes[i],
			sensor.passage.observed == State::Blocked, now, settings_, world.bodyPattern);
		const bool opening = sensor.newOpening && sensor.guidance.openingSafe
			&& sensor.probe.clearDepth >= sensor.wallMemory.RequiredDepth();
		events[i] = sensor.passageState.Observe(sensor.passage, opening, now, settings_.stableMs);
		if (!sensor.passage.available) {
			sensor.lastCue = State::Unknown;
			sensor.retry = false;
		}
		sensor.retrying = false;
		const auto failure = audio.failed[i];
		if (sensor.lastEmit && failure.eventMs == sensor.lastEmit && failure.state == sensor.lastCue) sensor.retry = !failure.submitted;
		const State lastCue = events[i] != State::Unknown ? events[i] : sensor.lastCue;
		sensor.cueState = sensor.passageState.Accepts(lastCue, sensor.passage, opening) ? lastCue : State::Unknown;
		if (events[i] == State::Unknown && sensor.retry && sensor.cueState == lastCue && lastCue != State::Unknown
			&& now - sensor.lastEmit >= settings_.retryMs) {
			events[i] = lastCue;
			sensor.retrying = true;
		}
		if (events[i] != State::Unknown) {
			sensor.lastEmit = now;
			sensor.lastCue = events[i];
			sensor.retry = false;
		}
	}
	for (size_t i = 0; i < sensors_.size(); ++i)
		sensors_[i].guidance.playbackMs = PlaybackBudget(audio, events, i);
	return events;
}
}
