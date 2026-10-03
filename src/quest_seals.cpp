#include "quest_seals.h"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <utility>

namespace D2RAccess::QuestSeals {

void Memory::Begin(uint64_t session) {
	if (session == m_session) return;
	m_session = session;
	m_states.fill(State::Unseen);
	m_opening = false;
}

void Memory::Observe(uint64_t session, uint8_t ordinal, bool opened) {
	if (!ordinal || ordinal > Count) return;
	Begin(session);
	State& state = m_states[ordinal - 1];
	if (state == State::Open) return; // a seal never shuts again
	if (opened) m_opening = m_opening || state == State::Shut;
	state = opened ? State::Open : State::Shut;
}

size_t Memory::Left(uint64_t session) const {
	if (session != m_session) return Count;
	return Count - static_cast<size_t>(std::count(m_states.begin(), m_states.end(), State::Open));
}

std::optional<size_t> Memory::TakeOpening(uint64_t session) {
	Begin(session);
	if (!std::exchange(m_opening, false)) return std::nullopt;
	return Left(session);
}

std::string LeftText(size_t left, const Strings::Table& strings) {
	if (!left) return strings.sealsAllOpen;
	char text[128]{};
	std::snprintf(text, sizeof text, strings.sealsLeft, static_cast<int>(left));
	return text;
}

namespace {
std::mutex g_mutex;
Memory g_memory;
}

void Observe(uint64_t session, uint8_t ordinal, bool opened) {
	const std::lock_guard lock(g_mutex);
	g_memory.Observe(session, ordinal, opened);
}
size_t Left(uint64_t session) {
	const std::lock_guard lock(g_mutex);
	return g_memory.Left(session);
}
std::optional<size_t> TakeOpening(uint64_t session) {
	const std::lock_guard lock(g_mutex);
	return g_memory.TakeOpening(session);
}
}
