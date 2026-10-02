#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// When the HUD's monster plate may speak. The game's auto-target hops between
// the monsters of a pack several times a second, naming each one far beyond
// the player's reach (log, 2026-09-29). Pure; docs/attack-reach.md.
namespace D2RAccess::MonsterNames {
struct Settings {
	// A name back on the plate this soon after it was last there stays silent.
	uint64_t repeatMs = 10000;
};

// Called on every reader poll: Admit while a monster label is on screen, Absent otherwise.
class Gate {
public:
	// `distance` and `important` describe the nearest bearer in sight, absent when
	// no unit carries the name; `reach` is absent when the attack reach is unknown.
	// Either absence drops the reach rule and keeps the repeat rule.
	bool Admit(std::string_view label, std::optional<double> distance, bool important,
		std::optional<double> reach, uint64_t now, const Settings& settings = {});
	void Absent() { current_.clear(); }
	void Reset() { shown_.clear(); current_.clear(); }
private:
	struct Shown { std::string label; uint64_t at = 0; };
	std::vector<Shown> shown_;
	std::string current_; // admitted, and on screen on every poll since
};

// The plate's affix separator dots spoken as a pause: "Demonio • Invoca" -> "Demonio, Invoca".
std::string Speakable(std::string_view label);
}
