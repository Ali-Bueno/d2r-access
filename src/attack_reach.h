#pragma once

#include "entity_cues.h"
#include "entity_radar.h"

#include <cstdint>
#include <optional>
#include <vector>

// How far the player's attacks reach, from the bound skills and the weapon in
// hand, and the moment a visible enemy comes within it. Pure; the rules and the
// game columns they read are in docs/attack-reach.md.
namespace D2RAccess::AttackReach {
enum class Style { Melee, Ranged };
enum class Mode { Auto, Melee, Ranged };
// skills.txt Range, as the compiled skill row stores it.
enum class SkillRange : uint8_t { None = 0, HandToHand = 1, Ranged = 2, Both = 3, Blank = 0xFF };
struct Skill {
	SkillRange range = SkillRange::Blank;
	bool damage = false; // the row carries its own damage: EType, EMin or MinDam
};
struct Loadout {
	std::vector<Skill> skills; // every bound skill, both bars
	bool missileWeapon = false; // a bow or crossbow in the active hands
	int rangeAdder = 0; // weapons.txt rangeadder of the melee weapon in hand
};
struct Settings {
	Mode mode = Mode::Auto;
	// Sound-design defaults, not game constants: a melee swing lands on a
	// monster beside the player, and a shot on one the enemy cues already play.
	double meleeReach = 3;
	double rangedReach = EntityRadar::Settings{}.range;
	double release = 1; // an enemy must back off this much further before it counts as out of reach
};

Style Classify(const Loadout& loadout);
Style Resolve(Mode mode, const Loadout& loadout);
double Reach(Style style, const Loadout& loadout, const Settings& settings);

struct Target {
	uint32_t id = 0;
	EntityCues::Bearing bearing;
};
// The nearest living enemy in sight within `reach`.
std::optional<Target> Nearest(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, double reach);

// The first enemy to come within reach while none was; nothing again until every enemy is out of reach.
class Tracker {
public:
	std::optional<Target> Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
		double reach, double release);
	void Reset() { inReach_ = false; }
private:
	bool inReach_ = false;
};
}
