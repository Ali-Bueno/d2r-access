#include "tutorial.h"

#include <atomic>
#include <utility>

namespace D2RAccess::Tutorial {
namespace {
std::atomic<Signals> g_posted{0};
std::atomic<bool> g_quietWorld{false}, g_busy{false};
}

Signals Needs(Step step) {
	switch (step) {
	case Step::Vitals: return Bit(Signal::Vitals);
	case Step::Radar: return Bit(Signal::Wall);
	case Step::Sounds: return Bit(Signal::HeardSounds);
	case Step::Scanner: return Bit(Signal::ScannedWarriv);
	case Step::MeetWarriv: return Bit(Signal::TalkedToWarriv);
	case Step::FindAkara: return Bit(Signal::ScannedAkara);
	case Step::Route: return Bit(Signal::RouteToAkara);
	case Step::Beacon: return Bit(Signal::AtAkara);
	case Step::Talk: return Bit(Signal::TalkedToAkara);
	case Step::LeaveTown: return Bit(Signal::InBloodMoor);
	case Step::Surroundings: return Bit(Signal::Terrain);
	case Step::Fight: return Bit(Signal::Kill);
	case Step::FindDen: return Bit(Signal::InDen);
	case Step::DenRooms: return Bit(Signal::RoomWays);
	case Step::ClearDen: return Bit(Signal::DenCleared);
	case Step::ReturnToTown: return Bit(Signal::InTown);
	// Akara's menu back from the Den, or the reward taken (PassedBy): the game may give it only at a
	// second talk (the user, 2026-11-02).
	case Step::Reward: return Bit(Signal::RewardTalk);
	case Step::Shop: return Bit(Signal::Shopped);
	case Step::SpendSkill: return Bit(Signal::SkillsSpent);
	case Step::Waypoint: return Bit(Signal::Waypoint);
	case Step::Finished: break;
	}
	return 0;
}

Signals PassedBy(Step step) {
	const Signals cleared = Bit(Signal::DenCleared) | Bit(Signal::DenDone);
	switch (step) {
	case Step::LeaveTown: case Step::Surroundings: case Step::Fight: case Step::FindDen: case Step::DenRooms: return cleared;
	case Step::ReturnToTown: case Step::Reward: return Bit(Signal::DenDone);
	// A shop already visited since the reward, or the point already spent: their words would be late.
	case Step::Shop: return Bit(Signal::Shopped);
	case Step::SpendSkill: return Bit(Signal::SkillsSpent);
	default: return 0;
	}
}

Step FromVersion(int version, int step) {
	if (step < 0) return Step::Finished;
	if (version < 2) {
		// Version 1 had no DenRooms nor Waypoint: its ClearDen (13) now begins at the Den's rooms.
		constexpr int OldReturnToTown = 14, OldFinished = 16;
		if (step >= OldFinished) return Step::Finished;
		if (step >= OldReturnToTown) ++step;
	}
	if (version < 3) {
		// Version 2 had no Shop nor SpendSkill: its Waypoint (17) and Finished (18) are two further on.
		constexpr int OldWaypoint = 17, OldFinished = 18;
		if (step >= OldFinished) return Step::Finished;
		if (step >= OldWaypoint) step += 2;
	}
	return step >= static_cast<int>(Step::Finished) ? Step::Finished : static_cast<Step>(step);
}

void Post(Signal signal) { g_posted.fetch_or(Bit(signal), std::memory_order_relaxed); }
Signals TakePosted() { return g_posted.exchange(0, std::memory_order_relaxed); }

void SetQuietWorld(bool quiet) { g_quietWorld.store(quiet, std::memory_order_relaxed); }
bool QuietWorld() { return g_quietWorld.load(std::memory_order_relaxed); }

void SetBusy(bool busy) { g_busy.store(busy, std::memory_order_relaxed); }
bool Busy() { return g_busy.load(std::memory_order_relaxed); }

void Guide::Start(Step step) {
	m_step = step;
	m_seen = 0;
}

void Guide::Notice(Signals events) { m_seen |= events; }

bool Guide::Done(Signals states) const {
	const Signals needs = Needs(m_step);
	return !Finished() && ((needs && ((m_seen | states) & needs) == needs) || Passed(states));
}

bool Guide::Passed(Signals states) const { return !Finished() && (states & PassedBy(m_step)) != 0; }

void Guide::Advance() {
	if (Finished()) return;
	m_step = static_cast<Step>(static_cast<size_t>(m_step) + 1);
	m_seen = 0;
}

void Audition::Start(std::vector<uint64_t> reads, uint64_t now, uint64_t gap, bool wait) {
	m_reads = std::move(reads);
	m_due = now;
	m_gap = gap;
	m_next = 0;
	m_heard.reset();
	m_wait = wait;
	m_phase = m_reads.empty() ? Phase::Done : Phase::Name;
}

Audition::Due Audition::Tick(uint64_t now, bool said) {
	if (now < m_due && !(said && m_phase == Phase::Sound)) return {};
	switch (m_phase) {
	case Phase::Name:
		m_phase = Phase::Sound;
		m_due = now + m_reads[m_next];
		return {Cue::Name, m_next};
	case Phase::Sound:
		m_heard = m_next++;
		Heard(now);
		return {Cue::Sound, *m_heard};
	case Phase::Gap: m_phase = m_next < m_reads.size() ? Phase::Name : Phase::Done; break;
	default: break;
	}
	return {};
}

void Audition::Heard(uint64_t now) {
	if (m_wait) m_phase = Phase::Hold;
	else { m_phase = Phase::Gap; m_due = now + m_gap; }
}

std::optional<size_t> Audition::Again() {
	if (!m_heard || m_phase == Phase::Idle || m_phase == Phase::Done) return std::nullopt;
	// A name already said for the next sound is said again when the lesson goes on.
	m_phase = Phase::Hold;
	return m_heard;
}

void Audition::Next(uint64_t now) {
	if (m_phase == Phase::Idle || m_phase == Phase::Done) return;
	if (m_phase == Phase::Hold) m_phase = m_next < m_reads.size() ? Phase::Name : Phase::Done;
	m_due = now;
}

void History::Add(std::string text) {
	m_messages.push_back(std::move(text));
	m_cursor = m_messages.size() - 1;
}

void History::Clear() {
	m_messages.clear();
	m_cursor = 0;
}

std::optional<History::Reading> History::Previous() {
	if (Empty()) return std::nullopt;
	if (m_cursor == 0) return At(Edge::First);
	--m_cursor;
	return At();
}

std::optional<History::Reading> History::Next() {
	if (Empty()) return std::nullopt;
	if (m_cursor + 1 >= m_messages.size()) return At(Edge::Last);
	++m_cursor;
	return At();
}

std::optional<History::Reading> History::First() {
	if (Empty()) return std::nullopt;
	m_cursor = 0;
	return At();
}

Offer::Reply Offer::Feed(Press press, uint64_t now) {
	if (const auto late = Tick(now); late != Reply::None) return late;
	if (press != Press::Other && !m_first) {
		m_first = press;
		m_at = now;
		return Reply::None;
	}
	const bool twice = m_first == press && press != Press::Other;
	m_first.reset();
	if (!twice) return Reply::Again;
	return press == Press::Yes ? Reply::Yes : press == Press::No ? Reply::No : Reply::Never;
}

Offer::Reply Offer::Tick(uint64_t now) {
	if (!m_first || now - m_at <= m_window) return Reply::None;
	m_first.reset();
	return Reply::Again;
}
}
