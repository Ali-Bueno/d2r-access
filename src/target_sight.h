#pragma once

#include "entity_radar.h"
#include "radar.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Is the entity the game just named actually in sight?
//
// D2R names a monster on its health plate, and an object or NPC on the world
// target label, from the game's own targeting, which happily reaches through
// walls: the player heard "Fallen" for a monster in the next room, one the
// right-stick scan would not list because the scan requires a terrain line of
// sight. This decides whether such a label may be spoken, from the same World
// and Entity list the scan uses, so both features agree on what "visible"
// means.
//
// The label is the game's own text ("Zombie. Undead"), not a unit id, so the
// entity behind it is identified by name, matched on word boundaries like the
// ground-item labels. Hiding needs positive evidence, exactly as in
// EntityRadar::LineOfSight: an unrecognised name, or a name whose only bearers
// are out of reach, stays Unknown and is spoken.
namespace D2RAccess::TargetSight {
enum class Sight { Unknown, Visible, Hidden };

struct Settings {
	// Collision subtiles. The game only labels what is on screen, and a legacy
	// 800x600 screen spans about forty subtiles, so a bearer of the name beyond
	// this cannot be the one the label is about.
	double reach = 40;
	bool enabled = true; // false speaks every label, as before this gate existed
};

// Whether `label` names an entity with a clear terrain line of sight.
// Ground items are ignored: the item proximity service owns their labels.
Sight Of(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	std::string_view label, const Settings& settings = {});

// The game's label carries the entity's name plus its own decoration
// ("Zombie. Undead", "Fallen Shaman. Demon 5 - Revives fallen"). True when
// `name` appears in `label` as a whole word.
bool Names(std::string_view label, std::string_view name);
}
