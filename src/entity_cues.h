#pragma once

#include "combat_target.h"
#include "entity_radar.h"

#include <array>
#include <optional>

namespace D2RAccess::EntityCues {
enum class Kind { Npc, Important, Resurrector, Ranged, Ordinary };
// Upper bound on simultaneous enemy voices; Settings::maxVoices picks the actual count.
inline constexpr size_t MaxVoices = 8, DefaultVoices = 6;
inline constexpr size_t KindCount = 5, MaxOrdinary = MaxVoices, SlotCount = KindCount - 1 + MaxOrdinary;
// Ranged attackers shoot from just past the screen edge, so they sound this far (subtiles) before they appear.
inline constexpr double RangedMargin = 3;
struct Settings {
	bool npcs = true, enemies = true, ranged = true, ordinary = true;
	double range = EntityRadar::Settings{}.range; // the cue radius when onScreen is off
	// Enemies sound once they are on the sighted player's screen, not within `range`.
	bool onScreen = true;
	CombatTarget::Screen screen;
	size_t maxVoices = DefaultVoices;
	// Listening feedback, 2026-09-22: much faster repeats, including mixed groups.
	// The far interval is longer than the 450 ms of three voices so six stay intelligible.
	uint64_t nearIntervalMs = 150, farIntervalMs = 600, gapMs = 100;
};
struct Bearing { double distance = 0, right = 0, front = 1; };
Bearing BearingOf(Radar::Position player, Radar::Position target);
struct Cue {
	uint32_t id = 0;
	Kind kind = Kind::Npc;
	bool boss = false; // an Important enemy that is an act boss or super unique, not a champion
	double distance = 0, right = 0, front = 1;
	uint64_t intervalMs = 0;
	size_t slot = 0;
};

// The nearest NPC plus up to `maxVoices` enemies: specials first, then the nearest ordinary ones.
std::vector<Cue> Select(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities, const Settings& settings);
// The town NPCs whose "!" has just appeared, nearest first: each once, again
// only after it was seen gone (talking to any NPC clears every one).
class AlertTracker {
public:
	std::vector<Cue> Raised(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities);
	void Reset() { shown_.clear(); }
private:
	std::vector<uint32_t> shown_;
};
class Tracker {
public:
	std::optional<Cue> Update(const Radar::World& world, const std::vector<EntityRadar::Entity>& entities,
		uint64_t now, const Settings& settings);
	void Reset();
private:
	// Class cadence survives nearest-target changes to avoid rapid alternating
	// pings when two NPCs or archers repeatedly exchange the nearest position.
	struct Slot { bool played = false; uint64_t last = 0; };
	std::array<Slot, SlotCount> slots_{};
	bool played_ = false;
	uint64_t last_ = 0;
};
// A Necromancer's usable corpses (a tester, 2026-10-14): a cue toward the nearest
// when some come within `range` after none were. They count as gone only once
// none is within CorpseLeaveFactor of the range, so one at the edge is not
// announced again with every step across it.
inline constexpr double CorpseLeaveFactor = 1.25;
// A usable-corpse read older than this, or of another player, is not used: a read
// that fails (a crowded area, a level change) leaves the last one in place.
inline constexpr uint64_t CorpsesFreshMs = 2000;
// How often the rattle repeats while corpses stay within range, in milliseconds; 0 is
// once as they come near. The mod starts at a drop sound's own pass (ItemAudio::DropPassMs).
inline constexpr std::array<int, 8> CorpseRepeatMs{0, 500, 750, 1000, 1500, 2000, 3000, 5000};
inline constexpr int DefaultCorpseRepeatLevel = 3;
size_t CorpsesWithin(Radar::Position player, const std::vector<Radar::Position>& corpses, double range);
class CorpseTracker {
public:
	struct Result {
		size_t count = 0;            // within range
		std::optional<Bearing> cue;  // toward the nearest, when they have just become available
	};
	// `repeatMs` above 0 cues again toward the nearest every so often while some stay within range.
	Result Update(Radar::Position player, const std::vector<Radar::Position>& corpses, double range,
		uint64_t now = 0, uint64_t repeatMs = 0);
	void Reset() { available_ = false; cuedMs_ = 0; }
private:
	bool available_ = false;
	uint64_t cuedMs_ = 0;
};
}
