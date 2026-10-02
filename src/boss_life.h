#pragma once

#include "vitals.h"

#include <cstdint>
#include <string>
#include <vector>

// A boss's life said the way the player's own is: on the same steps, the real
// percentage, critical falls interrupting. Pure; docs/boss-life.md.
namespace D2RAccess::BossLife {
struct Reading {
	uint32_t id = 0;
	std::string name;
	int percent = -1; // the game's own health plate fill; 0 once dead
};
struct Alert {
	std::string name;
	int percent = -1;
	bool interrupt = false;
};

class Tracker {
public:
	// `readings` covers every boss to follow now; one missing from it is forgotten,
	// and one seen for the first time only takes a silent baseline.
	std::vector<Alert> Update(const std::vector<Reading>& readings);
	// The bosses being followed, last percentage included, so a caller can look
	// for one that has just died and dropped out of the living units.
	std::vector<Reading> Followed() const;
	void Reset() { followed_.clear(); }
private:
	struct Boss { uint32_t id = 0; std::string name; Vitals::Meter meter; };
	std::vector<Boss> followed_;
};
}
