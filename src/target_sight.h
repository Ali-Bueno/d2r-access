#pragma once

#include "entity_radar.h"
#include "radar.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Is the entity the game just named actually in sight? The game's targeting
// reaches through walls; the right-stick scan does not, and this makes the HUD
// labels agree with the scan. Identifies the entity by name because the label
// is the game's own text, and hides only on positive evidence. Rationale and
// the live report behind it: docs/target-sight.md.
namespace D2RAccess::TargetSight {
enum class Sight { Unknown, Visible, Hidden };

struct Settings {
	// Collision subtiles: what a legacy 800x600 screen spans, and the game only
	// labels what is on screen.
	double reach = 40;
	bool enabled = true; // false speaks every label, as before this gate existed
};

// The unit the game itself targets (NativeHover), when it can be read: of the
// bearers of a label it is the one meant, not the nearest.
struct Aim {
	uint8_t unit = EntityRadar::Entity::NoUnit; // the game's unit type
	uint32_t id = 0;
};

// The nearest entity `label` names that is in sight, or why there is none.
// Ground items are ignored: the item proximity service owns their labels.
struct Bearer {
	Sight sight = Sight::Unknown;
	EntityRadar::Kind kind = EntityRadar::Kind::Item; // meaningful only when Visible
	bool important = false; // a boss or an elite
	double distance = 0;
	uint32_t id = 0; // the unit, when Visible: two barrels share a label but not an id
	// The scan's own name where it says more than the label (a Cairn Stone's
	// symbol, a seal's number) and the label stands for that one unit alone: the
	// game's target, or a bearer clearly nearer than any other. Empty otherwise.
	std::string name, worldName;
};
Bearer Find(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	std::string_view label, const Settings& settings = {}, const std::optional<Aim>& aim = std::nullopt);
// `label` with the bearer's own name in place of the game's: "Cairn Stone, one".
std::string Spoken(std::string_view label, const Bearer& bearer);
// Whether `label` names an entity with a clear terrain line of sight.
Sight Of(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	std::string_view label, const Settings& settings = {});

// True when `name` appears in `label` as a whole word: the label carries the
// game's own decoration too ("Zombie. Undead").
bool Names(std::string_view label, std::string_view name);
}
