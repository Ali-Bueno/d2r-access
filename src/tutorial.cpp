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
	case Step::Surroundings: return Bit(Signal::RoomWays) | Bit(Signal::Terrain);
	case Step::Fight: return Bit(Signal::Gold);
	case Step::FindDen: return Bit(Signal::InDen);
	case Step::ClearDen: return Bit(Signal::DenCleared);
	case Step::ReturnToTown: return Bit(Signal::InTown);
	case Step::Reward: return Bit(Signal::DenDone);
	case Step::Finished: break;
	}
	return 0;
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
	return !Finished() && needs && ((m_seen | states) & needs) == needs;
}

void Guide::Advance() {
	if (Finished()) return;
	m_step = static_cast<Step>(static_cast<size_t>(m_step) + 1);
	m_seen = 0;
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
	if (m_cursor == 0) return Reading{m_messages[m_cursor], Edge::First};
	return Reading{m_messages[--m_cursor]};
}

std::optional<History::Reading> History::Next() {
	if (Empty()) return std::nullopt;
	if (m_cursor + 1 >= m_messages.size()) return Reading{m_messages[m_cursor], Edge::Last};
	return Reading{m_messages[++m_cursor]};
}

std::optional<History::Reading> History::First() {
	if (Empty()) return std::nullopt;
	m_cursor = 0;
	return Reading{m_messages[m_cursor]};
}
}
