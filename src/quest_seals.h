#pragma once

#include "generated/lootables.h"
#include "speech_strings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// The Chaos Sanctuary's seals in one game, from the modes of the seal objects
// the scan reads: Diablo comes once all of them are open. docs/quest-objects.md
namespace D2RAccess::QuestSeals {
inline constexpr size_t Count = Generated::SealObjects.size();

class Memory {
public:
	// `ordinal` is Generated::SealOrdinal; `opened` is any mode past neutral.
	void Observe(uint64_t session, uint8_t ordinal, bool opened);
	// Seals not seen open in this game, the unseen ones included.
	size_t Left(uint64_t session) const;
	// The seals left, once, after a seal seen shut has been seen open.
	std::optional<size_t> TakeOpening(uint64_t session);

private:
	enum class State : uint8_t { Unseen, Shut, Open };
	void Begin(uint64_t session);
	uint64_t m_session = 0;
	std::array<State, Count> m_states{};
	bool m_opening = false;
};

// "Seals left: 3", or that all are open.
std::string LeftText(size_t left, const Strings::Table& strings);

// The one memory the object scan feeds and speech reads, from any thread.
void Observe(uint64_t session, uint8_t ordinal, bool opened);
size_t Left(uint64_t session);
std::optional<size_t> TakeOpening(uint64_t session);
}
