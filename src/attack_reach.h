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
	// Sound-design defaults, not game constants, for how far a monster the HUD
	// plate names may stand; the contact ping uses the game's own rule below.
	double meleeReach = 3;
	double rangedReach = EntityRadar::Settings{}.range;
};

Style Classify(const Loadout& loadout);
Style Resolve(Mode mode, const Loadout& loadout);
double Reach(Style style, const Loadout& loadout, const Settings& settings);

// The game's melee rule: 0x348650 (D2MOO UNITS_IsInMeleeRange) without its own
// collision test, for which the radar's line of sight stands in. Distances are
// half-subtiles. docs/controller-targeting.md#melee-reach
inline constexpr int PlayerSize = 2;  // every player's SizeX
inline constexpr int MonsterSize = 2; // MonStats2 SizeX is not read: the Fallen, their shamans and zombies are 2
inline constexpr uint32_t WarlockClass = 7;
inline constexpr int WarlockMeleeBonus = 2;
// 0x325200 (D2Common_10399): how far apart two units' footprints are, `dx` and `dy` in whole subtiles.
int UnitDistance(int dx, int dy, int sizeA, int sizeB);
// The player's melee range as 0x34A620 counts it: the weapon's rangeadder, two more for a Warlock.
int MeleeRange(const Loadout& loadout, std::optional<uint32_t> playerClass);
bool InMeleeRange(int distance, int meleeRange, int bonus = 0);

struct Contact {
	int meleeRange = 0;
	int release = 2; // sound design: an enemy must back off this much further before it counts as gone
};

struct Target {
	uint32_t id = 0;
	EntityCues::Bearing bearing;
	int distance = 0; // UnitDistance
};
// The nearest living enemy in sight the player could strike in melee now, `extra` widening the range.
std::optional<Target> Nearest(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
	const Contact& contact, int extra = 0);

// The first enemy to come within melee range while none was; nothing again until every enemy is out of it.
class Tracker {
public:
	std::optional<Target> Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
		const Contact& contact);
	void Reset() { inReach_ = false; }
private:
	bool inReach_ = false;
};
}
