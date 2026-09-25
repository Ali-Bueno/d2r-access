#include "explored_store.h"

#include <algorithm>
#include <chrono>
#include <set>

namespace D2RAccess::ExploredStore {
namespace {
std::vector<ExploredRooms::Room> UnionRooms(const std::vector<ExploredRooms::Room>& a,
	const std::vector<ExploredRooms::Room>& b) {
	std::map<std::pair<int, int>, bool> rooms;
	for (const auto& room : a) rooms[{room.tileX, room.tileY}] |= room.visited;
	for (const auto& room : b) rooms[{room.tileX, room.tileY}] |= room.visited;
	std::vector<ExploredRooms::Room> result;
	result.reserve(rooms.size());
	for (const auto& [point, visited] : rooms) result.push_back({point.first, point.second, visited});
	return result;
}
bool SameRooms(const std::vector<ExploredRooms::Room>& a, const std::vector<ExploredRooms::Room>& b) {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i].tileX != b[i].tileX || a[i].tileY != b[i].tileY || a[i].visited != b[i].visited) return false;
	return true;
}
// The record's size in the archive (explored_store.cpp Encode).
size_t Bytes(const Store::Record& record) {
	size_t bytes = 30 + 9 * record.rooms.size();
	for (const auto& place : record.places) bytes += 35 + place.name.size();
	return bytes;
}
void Bound(Store::Table& table) {
	size_t bytes = 20;
	for (const auto& [key, record] : table) bytes += Bytes(record);
	while (!table.empty() && (table.size() > MaxEntries || bytes > MaxArchiveBytes)) {
		const auto oldest = std::min_element(table.begin(), table.end(), [](const auto& a, const auto& b) {
			return a.second.touched < b.second.touched;
		});
		bytes -= Bytes(oldest->second);
		table.erase(oldest);
	}
}
void MergeFromDisk(Store::Table& current, const Store::Table& loaded, uint64_t& clock) {
	for (const auto& [key, record] : loaded) {
		auto& cached = current[key];
		cached.rooms = UnionRooms(record.rooms, cached.rooms);
		auto places = record.places; // what this session found is newer than the disk
		MergePlaces(places, cached.places);
		cached.places = std::move(places);
		cached.touched = std::max(record.touched, cached.touched);
		clock = std::max(clock, cached.touched);
	}
}
}

Store::~Store() { Stop(); }
bool Store::Start(std::filesystem::path directory) {
	if (thread_.joinable() || directory.empty()) return false;
	{ std::lock_guard lock(mutex_); table_.clear(); clock_ = generation_ = 0; }
	directory_ = std::move(directory);
	stopping_ = ready_ = failed_ = dirty_ = false;
	try { thread_ = std::thread(&Store::Run, this); }
	catch (...) { failed_ = true; return false; }
	return true;
}
bool Store::Ready() const { std::lock_guard lock(mutex_); return ready_; }
bool Store::Failed() const { std::lock_guard lock(mutex_); return failed_; }
bool Store::Load(Key key, ExploredRooms::Memory& memory, std::vector<Place>* places) const {
	std::lock_guard lock(mutex_);
	if (!ready_) return false;
	const auto found = table_.find(key);
	if (found == table_.end()) return false;
	memory.Merge(key.level, found->second.rooms);
	if (places) *places = found->second.places;
	return true;
}
void Store::Submit(std::vector<Update> updates) {
	std::lock_guard lock(mutex_);
	if (stopping_) return;
	for (auto& update : updates) {
		if (update.rooms.empty() && update.places.empty()) continue;
		if (update.rooms.size() > (MaxArchiveBytes - 48) / 9) { failed_ = true; continue; }
		auto& record = table_[update.key];
		auto merged = UnionRooms(record.rooms, update.rooms);
		auto places = record.places;
		const bool newPlaces = MergePlaces(places, update.places);
		if (SameRooms(record.rooms, merged) && !newPlaces) continue;
		record.rooms = std::move(merged);
		record.places = std::move(places);
		record.touched = ++clock_;
		++generation_;
		dirty_ = true;
	}
	Bound(table_);
	wake_.notify_one();
}
void Store::Stop() {
	{
		std::lock_guard lock(mutex_);
		stopping_ = true;
	}
	wake_.notify_all();
	if (thread_.joinable()) thread_.join();
}
void Store::Run() {
	Table loaded;
	bool cleanupPending = false;
	while (!MigrateDirectory(directory_, &cleanupPending) || !ReadTable(directory_, loaded)) {
		std::unique_lock lock(mutex_);
		failed_ = true;
		if (stopping_) return; // a failed disk cannot hold plugin shutdown hostage
		const uint64_t version = generation_;
		wake_.wait_for(lock, std::chrono::seconds(30), [&] { return stopping_ || generation_ != version; });
	}
	{
		std::lock_guard lock(mutex_);
		{
			std::set<Key> pendingKeys;
			for (const auto& [key, record] : table_) pendingKeys.insert(key);
			MergeFromDisk(table_, loaded, clock_);
			for (const auto& key : pendingKeys) table_[key].touched = ++clock_;
			Bound(table_);
			ready_ = true;
			failed_ = cleanupPending;
		}
	}
	while (true) {
		Table snapshot;
		uint64_t version = 0;
		{
			std::unique_lock lock(mutex_);
			if (cleanupPending) wake_.wait_for(lock, std::chrono::seconds(30), [&] { return dirty_ || stopping_; });
			else wake_.wait(lock, [&] { return dirty_ || stopping_; });
			if (!dirty_ && stopping_) break;
			if (!dirty_) {
				lock.unlock();
				bool stillPending = false;
				Table migratedRecords;
				const bool migrated = MigrateDirectory(directory_, &stillPending) && ReadTable(directory_, migratedRecords);
				lock.lock();
				if (migrated) {
					MergeFromDisk(table_, migratedRecords, clock_);
					Bound(table_);
				}
				cleanupPending = !migrated || stillPending;
				failed_ = cleanupPending;
				continue;
			}
			snapshot = table_;
			version = generation_;
		}
		const bool written = WriteTable(directory_, snapshot);
		std::unique_lock lock(mutex_);
		failed_ = !written || cleanupPending;
		if (written) {
			if (generation_ == version) { table_ = std::move(snapshot); dirty_ = false; }
			continue;
		}
		if (stopping_) break;
		wake_.wait_for(lock, std::chrono::seconds(30), [&] { return stopping_ || generation_ != version; });
	}
}
}
