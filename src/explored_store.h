#pragma once

#include "explored_rooms.h"
#include "native_level_exits.h"

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

// One bounded archive for the room states of many generated maps. Keys retain
// the old seed/level/layout identity; old per-level files migrate on the worker.
namespace D2RAccess::ExploredStore {
struct Key {
	uint32_t seed = 0, level = 0;
	uint64_t digest = 0;
	bool operator<(const Key& other) const;
};
struct Update { Key key; std::vector<ExploredRooms::Room> rooms; };
inline constexpr size_t MaxEntries = 400, MaxArchiveBytes = 4 * 1024 * 1024;
uint64_t LayoutDigest(const std::vector<NativeLevelExits::LevelRoom>& rooms);
std::filesystem::path ArchiveFile(const std::filesystem::path& directory);

class Store {
public:
	struct Record { std::vector<ExploredRooms::Room> rooms; uint64_t touched = 0; };
	using Table = std::map<Key, Record>;
	~Store();
	bool Start(std::filesystem::path directory);
	bool Ready() const;
	bool Failed() const;
	bool Load(Key key, ExploredRooms::Memory& memory) const; // cached memory only
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
bool ReadArchive(const std::filesystem::path& directory, Key key, ExploredRooms::Memory& memory);
}
