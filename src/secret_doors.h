#pragma once

#include "radar.h"

#include <vector>

// The Jail's secret doors: wall panels a sighted player finds by a pattern in the wall. A
// player walking past a shut one feels a draft, once, instead of being told. Pure logic.
// docs/secret-doors.md
namespace D2RAccess::SecretDoors {

// Collision subtiles: about two tiles, the reach of a sighted player's eye along a wall.
inline constexpr double FeltWithin = 10;
// A door felt is felt again once the player has gone this far from it.
inline constexpr double FeltAgainBeyond = 20;

class Draft {
public:
	// The shut secret doors loaded this tick; true when the player has just come within
	// reach of one not felt since they last went away from it.
	bool Felt(const std::vector<Radar::Position>& doors, Radar::Position player);
	void Reset() { m_felt.clear(); }

private:
	std::vector<Radar::Position> m_felt;
};

}
