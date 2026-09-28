#pragma once

#include "entity_radar.h"
#include "radar.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Where the player last died on each map, and the way back to it over the ways
// out already found. Pure logic: the service records, persists and lists it.
// `docs/death-place.md`
namespace D2RAccess::DeathPlace {

// The death place's identity in the scanner and the beacon; no position identity reaches it.
inline constexpr uint64_t Identity = UINT64_MAX;

struct Record {
	uint32_t seed = 0; // NativeDrlg::MapSeed: one map, whatever its level
	int level = 0;
	Radar::Position position;
	bool operator==(const Record& other) const {
		return seed == other.seed && level == other.level && position.x == other.position.x && position.y == other.position.y;
	}
};

// One record per map, newest first. A death replaces its map's record; the
// oldest maps are dropped past MaxRecords.
class Memory {
public:
	// Enough for every character a player keeps; one line each on disk.
	static constexpr size_t MaxRecords = 64;
	void Died(const Record& record);
	const Record* Find(uint32_t seed) const;
	bool Clear(uint32_t seed); // true when the map had a record
	// "seed level x y" per line; Parse keeps the valid lines of a damaged file.
	std::string Serialize() const;
	void Parse(std::string_view text);
	const std::vector<Record>& Records() const { return m_records; }
private:
	std::vector<Record> m_records;
};

// Each level's known ways out: exits with their destination level, and waypoints.
using KnownPlaces = std::map<int, std::vector<EntityRadar::Entity>>;

struct Step {
	EntityRadar::Entity landmark; // on the current level: an exit, or its waypoint
	int next = 0;                 // the level it leads to; for a waypoint, the one to pick
	bool waypoint = false;
};

// The first step from `current` toward `target`: the exit starting the fewest
// level changes over known exits, the nearest to `player` among equals. With
// no such route, the current level's waypoint to the known waypoint nearest to
// `target` by exits. Nothing when neither is known.
std::optional<Step> NextStep(int current, int target, const KnownPlaces& known, Radar::Position player);

// The death place as a scanner landmark in the corpse category.
EntityRadar::Entity Landmark(const Record& record, std::string name);

}
