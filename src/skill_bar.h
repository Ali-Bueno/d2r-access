#pragma once
#include <cstdint>
#include <span>
#include <string>

// Holding LT alone reads the six LT-bar skill slots, as the HUD shows them to
// a sighted player while LT is held. See docs/skill-bar.md.
namespace D2RAccess::SkillBar {
// Agreed with the user (2026-09-25): long enough that an LT+button combo never
// reads the bar, short enough to feel like a glance at the HUD.
inline constexpr uint64_t PeekHoldMs = 600;

enum class Action { None, Speak, Stop };

// Once per UI tick: when the bar is read, and when releasing LT cuts it. A
// hold that ever includes another button, or starts outside play, stays silent.
class PeekGate {
public:
	Action Update(uint64_t now, bool playing, bool held, bool otherButton);
private:
	enum class Phase { Idle, Waiting, Speaking, Spent };
	Phase phase = Phase::Idle;
	uint64_t since = 0;
};

struct Slot {
	std::string button;  // spoken control name; the slot is skipped without one
	bool bound = false;
	std::string skill;   // the game's skill name; a bound slot without one is skipped
	int quantity = 0;    // the count the HUD shows; 0 when it shows none
};
// The slots in HUD order as one phrase, empty slots included.
std::string Compose(std::span<const Slot> slots);
}
