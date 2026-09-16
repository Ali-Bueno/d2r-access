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
void Engine::Suspend() {
	// Pauses, a missing sample and discontinuous movement invalidate live
	// evidence, not the positions of previously announced turns in this area.
	for (auto& sensor : sensors_) {
		auto memory = std::move(sensor.openingVisit);
		memory.Suspend();
		sensor = {};
		sensor.openingVisit = std::move(memory);
	}
	motion_.clear(); velocity_ = {}; lastMove_ = 0;
	positioned_ = false;
	++revision_;
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
		if (world.session == session_ && world.level == level_ && now >= lastTick_) Suspend();
		else Reset();
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
			sensor.passage.observed == State::Blocked, now, settings_, &structure);
		sensor.openingVisit.Update(world.player,Axes[i],sensor.structure,sensor.probe,
			sensor.guidance.openingSafe && sensor.passage.observed == State::Open,world.bodyPattern,now,settings_);
		const bool revisit = sensor.openingVisit.Ready();
		// A real wall returning between two visits rearms an announced exit
		// even when the player stays outside the proximity-warning distance.
		if (sensor.wallMemory.BarrierConfirmed() || revisit) sensor.passageState.RearmOpening(revisit);
		const bool opening = revisit || (sensor.newOpening && sensor.guidance.openingSafe
			&& sensor.probe.clearDepth >= sensor.wallMemory.RequiredDepth());
		events[i] = sensor.passageState.Observe(sensor.passage, opening, now, settings_.stableMs);
		if (events[i] == State::Open) {
			if (revisit) sensor.openingVisit.Reported();
			else sensor.openingVisit.Remember(world.player,Axes[i],sensor.wallMemory.RequiredDepth(),&structure);
		}
		if (events[i] != State::Unknown) sensor.revisitCue = events[i] == State::Open && revisit;
		if (!sensor.passage.available) {
			sensor.lastCue = State::Unknown;
			sensor.revisitCue = false;
			sensor.retry = false;
		}
		sensor.retrying = false;
		const auto failure = audio.failed[i];
		if (sensor.lastEmit && failure.eventMs == sensor.lastEmit && failure.state == sensor.lastCue) sensor.retry = !failure.submitted;
		const State lastCue = events[i] != State::Unknown ? events[i] : sensor.lastCue;
		// A consumed reminder remains valid for queue/retry delivery while its
		// remembered approach is clear, without rearming another announcement.
		const bool cueOpening = opening || (sensor.revisitCue && sensor.openingVisit.Valid());
		sensor.cueState = sensor.passageState.Accepts(lastCue, sensor.passage, cueOpening) ? lastCue : State::Unknown;
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
