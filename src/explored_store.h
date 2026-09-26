#pragma once

#include "explored_rooms.h"
#include "native_level_exits.h"

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// One bounded archive for the room states of many generated maps, and the
// fixed places found on each. Keys retain the old seed/level/layout identity;
// old per-level files migrate on the worker.
namespace D2RAccess::ExploredStore {
struct Key {
	uint32_t seed = 0, level = 0;
	uint64_t digest = 0;
	bool operator<(const Key& other) const;
};
// A fixed place found on one map: an exit, a waypoint, the stash
// (MapSurvey::Fixed). Kept with that map's rooms, so a later game on the same
// map knows it from anywhere, and a regenerated map never does.
// `docs/exploration.md`
struct Place {
	uint32_t id = 0;          // the scanner's selection id
	uint64_t identity = 0;    // EntityRadar::Entity::placeId; one position is one place
	double x = 0, y = 0;      // subtiles
	uint32_t destination = 0; // level id for an exit, 0 when unknown
	std::string name;         // as the game named it when found, in that game's language
	uint8_t kind = 0, category = 0; // EntityRadar::Kind and Category, checked on use
	std::string nameKey;      // the game string key of `name`, read again in the language being played
	bool operator==(const Place&) const = default;
};
struct Update { Key key; std::vector<ExploredRooms::Room> rooms; std::vector<Place> places; };
inline constexpr size_t MaxEntries = 400, MaxArchiveBytes = 4 * 1024 * 1024;
// A level has a handful of fixed places; the bounds only keep a damaged record small.
inline constexpr size_t MaxPlaces = 64, MaxPlaceName = 255;
// Adds the places of `found` new to `known`; one already known at the same
// position takes the newer name, destination and kind. Unplaceable or
// oversized places are skipped. True when `known` changed.
bool MergePlaces(std::vector<Place>& known, const std::vector<Place>& found);
// How a place just seen stands against the map's list: known, the way back to
// the level the player came from (`cameFrom`, 0 when unknown), or a new find.
enum class Novelty { Known, WayBack, Find };
Novelty Classify(const std::vector<Place>& known, const Place& place, uint32_t cameFrom);
uint64_t LayoutDigest(const std::vector<NativeLevelExits::LevelRoom>& rooms);
std::filesystem::path ArchiveFile(const std::filesystem::path& directory);

class Store {
public:
	struct Record { std::vector<ExploredRooms::Room> rooms; uint64_t touched = 0; std::vector<Place> places; };
	using Table = std::map<Key, Record>;
	~Store();
	bool Start(std::filesystem::path directory);
	bool Ready() const;
	bool Failed() const;
	// Cached memory only. `places`, when given, receives the map's stored places.
	bool Load(Key key, ExploredRooms::Memory& memory, std::vector<Place>* places = nullptr) const;
	void Submit(std::vector<Update> updates); // immutable snapshots; no disk I/O
	void Stop(); // joins the worker after its final write attempt
private:
	void Run();
	std::filesystem::path directory_;
	mutable std::mutex mutex_;
	std::condition_variable wake_;
	std::thread thread_;
	Table table_;
	uint64_t clock_ = 0, generation_ = 0;
	bool ready_ = false, failed_ = false, dirty_ = false, stopping_ = false;
};
// Disk operations are for the store worker, tests and explicit game-closed migration.
bool ReadTable(const std::filesystem::path& directory, Store::Table& table);
bool WriteTable(const std::filesystem::path& directory, Store::Table& table);
bool MigrateDirectory(const std::filesystem::path& directory, bool* cleanupIncomplete = nullptr);
bool ReadArchive(const std::filesystem::path& directory, Key key, ExploredRooms::Memory& memory,
	std::vector<Place>* places = nullptr);
}
