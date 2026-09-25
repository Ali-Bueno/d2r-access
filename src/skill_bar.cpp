#include "skill_bar.h"
#include "speech_strings.h"
#include <cstdio>

namespace D2RAccess::SkillBar {
Action PeekGate::Update(uint64_t now, bool playing, bool held, bool otherButton) {
	if (!held) {
		const bool speaking = phase == Phase::Speaking;
		phase = Phase::Idle;
		return speaking ? Action::Stop : Action::None;
	}
	switch (phase) {
	case Phase::Idle:
		phase = playing && !otherButton ? Phase::Waiting : Phase::Spent;
		since = now;
		return Action::None;
	case Phase::Waiting:
		if (otherButton || !playing) { phase = Phase::Spent; return Action::None; }
		if (now - since < PeekHoldMs) return Action::None;
		phase = Phase::Speaking;
		return Action::Speak;
	case Phase::Speaking:
		// A combo after listening casts; the reading gives way to it. A menu
		// that opened has its own announcement, which already interrupts.
		if (!otherButton && playing) return Action::None;
		phase = Phase::Spent;
		return otherButton ? Action::Stop : Action::None;
	case Phase::Spent:
		return Action::None;
	}
	return Action::None;
}

std::string Compose(std::span<const Slot> slots) {
	const auto& strings = Strings::Current();
	std::string text;
	for (const Slot& slot : slots) {
		if (slot.button.empty() || (slot.bound && slot.skill.empty())) continue;
		std::string value = slot.bound ? slot.skill : strings.slotEmpty;
		if (slot.bound && slot.quantity > 0) value += strings.listSeparator + std::to_string(slot.quantity);
		char phrase[256]{};
		std::snprintf(phrase, sizeof(phrase), strings.skillBarSlot, slot.button.c_str(), value.c_str());
		if (!text.empty()) text += ". ";
		text += phrase;
	}
	return text;
}
}
