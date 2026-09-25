#include "explored_store.h"

#include <windows.h>
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <regex>
#include <system_error>
#include <tuple>

namespace D2RAccess::ExploredStore {
namespace {
constexpr uint64_t FnvOffset = 14695981039346656037ull, FnvPrime = 1099511628211ull;
// Version 2 adds each map's fixed places after its rooms, version 3 each place's
// name key; version 1 and 2 archives still read.
constexpr char Magic[] = "D2REXP03", MagicNoKeys[] = "D2REXP02", MagicRoomsOnly[] = "D2REXP01";
using Table = Store::Table;
using Room = ExploredRooms::Room;
void Fnv(uint64_t& hash, uint8_t byte) { hash ^= byte; hash *= FnvPrime; }
void Put(std::vector<uint8_t>& bytes, uint64_t value, int count) {
	for (int i = 0; i < count; ++i) bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
}
bool Get(const std::vector<uint8_t>& bytes, size_t& at, uint64_t& value, int count) {
	if (count < 0 || at > bytes.size() || static_cast<size_t>(count) > bytes.size() - at) return false;
	value = 0;
	for (int i = 0; i < count; ++i) value |= uint64_t(bytes[at++]) << (i * 8);
	return true;
}
uint64_t Checksum(const std::vector<uint8_t>& bytes, size_t end) {
	uint64_t hash = FnvOffset;
	for (size_t i = 0; i < end; ++i) Fnv(hash, bytes[i]);
	return hash;
}
bool SameRooms(const std::vector<Room>& a, const std::vector<Room>& b) {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i].tileX != b[i].tileX || a[i].tileY != b[i].tileY || a[i].visited != b[i].visited) return false;
	return true;
}
std::vector<Room> UnionRooms(const std::vector<Room>& a, const std::vector<Room>& b) {
	std::map<std::pair<int, int>, bool> rooms;
	for (const auto& room : a) rooms[{room.tileX, room.tileY}] |= room.visited;
	for (const auto& room : b) rooms[{room.tileX, room.tileY}] |= room.visited;
	std::vector<Room> result;
	result.reserve(rooms.size());
	for (const auto& [position, visited] : rooms) result.push_back({position.first, position.second, visited});
	return result;
}
bool Placeable(const Place& place) {
	return place.identity && place.name.size() <= MaxPlaceName && place.nameKey.size() <= MaxPlaceName
		&& std::isfinite(place.x) && std::isfinite(place.y);
}
bool Encode(const Table& table, std::vector<uint8_t>& bytes) {
	bytes.assign(Magic, Magic + 8);
	Put(bytes, table.size(), 4);
	for (const auto& [key, entry] : table) {
		if (entry.rooms.size() > (MaxArchiveBytes - 20) / 9) return false;
		Put(bytes, key.seed, 4); Put(bytes, key.level, 4); Put(bytes, key.digest, 8);
		Put(bytes, entry.touched, 8); Put(bytes, entry.rooms.size(), 4);
		for (const auto& room : entry.rooms) {
			Put(bytes, static_cast<uint32_t>(room.tileX), 4);
			Put(bytes, static_cast<uint32_t>(room.tileY), 4);
			bytes.push_back(room.visited ? 2 : 1);
		}
		if (entry.places.size() > MaxPlaces) return false;
		Put(bytes, entry.places.size(), 2);
		for (const auto& place : entry.places) {
			if (!Placeable(place)) return false;
			Put(bytes, place.id, 4); Put(bytes, place.identity, 8);
			Put(bytes, std::bit_cast<uint64_t>(place.x), 8); Put(bytes, std::bit_cast<uint64_t>(place.y), 8);
			Put(bytes, place.destination, 4); Put(bytes, place.kind, 1); Put(bytes, place.category, 1);
			Put(bytes, place.name.size(), 1);
			bytes.insert(bytes.end(), place.name.begin(), place.name.end());
			Put(bytes, place.nameKey.size(), 1);
			bytes.insert(bytes.end(), place.nameKey.begin(), place.nameKey.end());
		}
		if (bytes.size() + 8 > MaxArchiveBytes) return false;
	}
	Put(bytes, Checksum(bytes, bytes.size()), 8);
	return bytes.size() <= MaxArchiveBytes;
}
bool Decode(const std::vector<uint8_t>& bytes, Table& table) {
	if (bytes.size() < 20 || bytes.size() > MaxArchiveBytes) return false;
	const bool withKeys = std::equal(Magic, Magic + 8, bytes.begin());
	const bool withPlaces = withKeys || std::equal(MagicNoKeys, MagicNoKeys + 8, bytes.begin());
	if (!withPlaces && !std::equal(MagicRoomsOnly, MagicRoomsOnly + 8, bytes.begin())) return false;
	size_t at = 8;
	uint64_t count = 0, word = 0;
	if (!Get(bytes, at, count, 4) || count > MaxEntries) return false;
	Table decoded;
	for (uint64_t i = 0; i < count; ++i) {
		Key key;
		Store::Record record;
		if (!Get(bytes, at, word, 4)) return false;
		key.seed = static_cast<uint32_t>(word);
		if (!Get(bytes, at, word, 4)) return false;
		key.level = static_cast<uint32_t>(word);
		if (!Get(bytes, at, key.digest, 8) || !Get(bytes, at, record.touched, 8)
			|| !Get(bytes, at, word, 4) || word > (MaxArchiveBytes - 20) / 9
			|| at + 8 > bytes.size() || word > (bytes.size() - at - 8) / 9) return false;
		record.rooms.reserve(static_cast<size_t>(word));
		for (uint64_t j = 0; j < word; ++j) {
			uint64_t x = 0, y = 0, flag = 0;
			if (!Get(bytes, at, x, 4) || !Get(bytes, at, y, 4) || !Get(bytes, at, flag, 1) || (flag != 1 && flag != 2)) return false;
			record.rooms.push_back({static_cast<int32_t>(x), static_cast<int32_t>(y), flag == 2});
		}
		if (withPlaces) {
			if (!Get(bytes, at, word, 2) || word > MaxPlaces) return false;
			for (uint64_t j = 0; j < word; ++j) {
				Place place;
				uint64_t id = 0, x = 0, y = 0, destination = 0, kind = 0, category = 0, length = 0;
				if (!Get(bytes, at, id, 4) || !Get(bytes, at, place.identity, 8) || !Get(bytes, at, x, 8) || !Get(bytes, at, y, 8)
					|| !Get(bytes, at, destination, 4) || !Get(bytes, at, kind, 1) || !Get(bytes, at, category, 1)
					|| !Get(bytes, at, length, 1) || length > bytes.size() - at) return false;
				place.id = static_cast<uint32_t>(id);
				place.x = std::bit_cast<double>(x);
				place.y = std::bit_cast<double>(y);
				place.destination = static_cast<uint32_t>(destination);
				place.kind = static_cast<uint8_t>(kind);
				place.category = static_cast<uint8_t>(category);
				place.name.assign(reinterpret_cast<const char*>(bytes.data() + at), static_cast<size_t>(length));
				at += static_cast<size_t>(length);
				if (withKeys) {
					if (!Get(bytes, at, length, 1) || length > bytes.size() - at) return false;
					place.nameKey.assign(reinterpret_cast<const char*>(bytes.data() + at), static_cast<size_t>(length));
					at += static_cast<size_t>(length);
				}
				if (!Placeable(place)) return false;
				record.places.push_back(std::move(place));
			}
		}
		if (!decoded.emplace(key, std::move(record)).second) return false;
	}
	if (at + 8 != bytes.size() || !Get(bytes, at, word, 8) || word != Checksum(bytes, bytes.size() - 8)) return false;
	table = std::move(decoded);
	return true;
}
bool ReadBytes(const std::filesystem::path& file, std::vector<uint8_t>& bytes) {
	std::error_code error;
	const auto size = std::filesystem::file_size(file, error);
	if (error || size > MaxArchiveBytes) return false;
	std::ifstream in(file, std::ios::binary);
	if (!in) return false;
	bytes.resize(static_cast<size_t>(size));
	if (!bytes.empty() && !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return false;
	return true;
}
bool ReadAt(const std::filesystem::path& file, Table& table) {
	std::vector<uint8_t> bytes;
	return ReadBytes(file, bytes) && Decode(bytes, table);
}
bool SameTables(const Table& a, const Table& b) {
	if (a.size() != b.size()) return false;
	for (const auto& [key, record] : a) {
		const auto found = b.find(key);
		if (found == b.end() || found->second.touched != record.touched || !SameRooms(found->second.rooms, record.rooms)
			|| found->second.places != record.places) return false;
	}
	return true;
}
void Trim(Table& table) {
	std::vector<uint8_t> bytes;
	while (table.size() > MaxEntries || !Encode(table, bytes)) {
		if (table.size() <= 1) break;
		const auto oldest = std::min_element(table.begin(), table.end(), [](const auto& a, const auto& b) {
			return a.second.touched < b.second.touched;
		});
		table.erase(oldest);
	}
}
bool WriteAt(const std::filesystem::path& directory, Table& table) {
	Table candidate = table;
	Trim(candidate);
	std::vector<uint8_t> bytes;
	if (!Encode(candidate, bytes)) return false;
	std::error_code error;
	std::filesystem::create_directories(directory, error);
	if (error) return false;
	const auto temp = directory / "explored.dat.tmp";
	const auto target = ArchiveFile(directory);
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		if (!out) return false;
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		out.flush();
		if (!out) return false;
	}
	Table checked;
	if (!ReadAt(temp, checked) || !SameTables(candidate, checked)) return false;
	if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
	if (!ReadAt(target, checked) || !SameTables(candidate, checked)) return false;
	table = std::move(candidate);
	return true;
}
bool LegacyName(const std::filesystem::path& path, Key& key) {
	static const std::regex pattern(R"(^explored-([0-9A-Fa-f]{8})-([0-9]+)-([0-9A-Fa-f]{16})\.txt$)");
	std::smatch match;
	const auto name = path.filename().string();
	if (!std::regex_match(name, match, pattern)) return false;
	try {
		const auto level = std::stoull(match[2].str());
		if (level > UINT32_MAX) return false;
		key = {static_cast<uint32_t>(std::stoul(match[1].str(), nullptr, 16)), static_cast<uint32_t>(level),
			std::stoull(match[3].str(), nullptr, 16)};
		return true;
	} catch (...) { return false; }
}
bool ReadLegacy(const std::filesystem::path& path, Key key, std::vector<Room>& rooms, bool& complete) {
	std::error_code error;
	const auto size = std::filesystem::file_size(path, error);
	if (error || size > MaxArchiveBytes) return false;
	std::ifstream in(path, std::ios::binary);
	if (!in) return false;
	complete = true;
	std::map<std::pair<int, int>, bool> parsed;
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::string_view remaining(line);
		const auto field = [&](auto& value) {
			while (!remaining.empty() && remaining.front() == ' ') remaining.remove_prefix(1);
			const auto result = std::from_chars(remaining.data(), remaining.data() + remaining.size(), value);
			if (result.ec != std::errc{}) return false;
			remaining.remove_prefix(static_cast<size_t>(result.ptr - remaining.data()));
			return true;
		};
		uint32_t level = 0;
		int x = 0, y = 0;
		if (!field(level) || !field(x) || !field(y)) { complete = false; continue; }
		while (!remaining.empty() && remaining.front() == ' ') remaining.remove_prefix(1);
		if (level != key.level || remaining.size() != 1 || (remaining.front() != 's' && remaining.front() != 'v')) {
			complete = false;
			continue;
		}
		const char flag = remaining.front();
		parsed[{x, y}] |= flag == 'v';
	}
	if (in.bad()) return false;
	rooms.clear();
	rooms.reserve(parsed.size());
	for (const auto& [point, visited] : parsed) rooms.push_back({point.first, point.second, visited});
	return true;
}
struct Legacy { Key key; std::filesystem::path path; std::vector<Room> rooms; bool complete = false; };
bool LegacyFiles(const std::filesystem::path& directory, std::vector<Legacy>& result) {
	std::error_code error;
	if (!std::filesystem::exists(directory, error)) return !error;
	if (error) return false;
	for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
		const auto& entry = *it;
		if (!entry.is_regular_file(error)) { if (error) return false; continue; }
		Key key;
		std::vector<Room> rooms;
		bool complete = false;
		if (!LegacyName(entry.path(), key)) continue;
		if (!ReadLegacy(entry.path(), key, rooms, complete)) return false;
		result.push_back({key, entry.path(), std::move(rooms), complete});
	}
	return !error;
}
}

bool MergePlaces(std::vector<Place>& known, const std::vector<Place>& found) {
	bool changed = false;
	for (const auto& place : found) {
		if (!Placeable(place)) continue;
		const auto same = std::find_if(known.begin(), known.end(), [&](const Place& entry) { return entry.identity == place.identity; });
		if (same != known.end()) {
			// The position is fixed; a loaded warp and its preset differ only in id and fraction.
			if (same->name != place.name || same->nameKey != place.nameKey || same->destination != place.destination
				|| same->kind != place.kind || same->category != place.category) {
				same->name = place.name;
				same->nameKey = place.nameKey;
				same->destination = place.destination;
				same->kind = place.kind;
				same->category = place.category;
				changed = true;
			}
		} else if (known.size() < MaxPlaces) {
			known.push_back(place);
			changed = true;
		}
	}
	return changed;
}
bool Key::operator<(const Key& other) const { return std::tie(seed, level, digest) < std::tie(other.seed, other.level, other.digest); }
std::filesystem::path ArchiveFile(const std::filesystem::path& directory) { return directory / "explored.dat"; }
uint64_t LayoutDigest(const std::vector<NativeLevelExits::LevelRoom>& rooms) {
	std::vector<NativeLevelExits::TileRect> rects;
	for (const auto& room : rooms) rects.push_back(room.rect);
	std::sort(rects.begin(), rects.end(), [](const auto& a, const auto& b) {
		return a.y != b.y ? a.y < b.y : a.x != b.x ? a.x < b.x : a.width != b.width ? a.width < b.width : a.height < b.height;
	});
	uint64_t hash = FnvOffset;
	for (const auto& rect : rects) for (const int value : {rect.x, rect.y, rect.width, rect.height})
		for (int shift = 0; shift < 32; shift += 8) Fnv(hash, static_cast<uint8_t>(static_cast<uint32_t>(value) >> shift));
	return hash;
}
bool ReadTable(const std::filesystem::path& directory, Table& table) {
	std::error_code error;
	if (!std::filesystem::exists(ArchiveFile(directory), error)) return !error;
	return !error && ReadAt(ArchiveFile(directory), table);
}
bool WriteTable(const std::filesystem::path& directory, Table& table) { return WriteAt(directory, table); }
bool MigrateDirectory(const std::filesystem::path& directory, bool* cleanupIncomplete) {
	if (cleanupIncomplete) *cleanupIncomplete = false;
	Table table;
	if (!ReadTable(directory, table)) return false; // never overwrite a corrupt or unsupported archive
	std::vector<Legacy> legacy;
	if (!LegacyFiles(directory, legacy)) return false;
	if (legacy.empty()) return true;
	uint64_t touched = 0;
	for (const auto& [key, record] : table) touched = std::max(touched, record.touched);
	for (const auto& file : legacy) {
		auto& record = table[file.key];
		record.rooms = UnionRooms(record.rooms, file.rooms);
		record.touched = ++touched;
	}
	if (!WriteAt(directory, table)) return false;
	Table verified;
	if (!ReadTable(directory, verified)) return false;
	bool cleaned = true;
	for (const auto& file : legacy) {
		const auto found = verified.find(file.key);
		if (!file.complete || found == verified.end() || !SameRooms(UnionRooms(found->second.rooms, file.rooms), found->second.rooms)) {
			cleaned = false;
			continue;
		}
		std::error_code error;
		std::filesystem::remove(file.path, error); // only the exact recognized file, after verified import
		if (error) cleaned = false;
	}
	if (cleanupIncomplete) *cleanupIncomplete = !cleaned;
	return true; // imported rooms remain usable if cleanup needs another attempt
}
bool ReadArchive(const std::filesystem::path& directory, Key key, ExploredRooms::Memory& memory, std::vector<Place>* places) {
	Table table;
	if (!ReadTable(directory, table)) return false;
	const auto found = table.find(key);
	if (found == table.end()) return false;
	memory.Merge(key.level, found->second.rooms);
	if (places) *places = found->second.places;
	return true;
}
}
