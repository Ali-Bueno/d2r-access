#include "explored_rooms.h"
#include "explored_store.h"
#include "frontier.h"
#include "test_support.h"

#include <filesystem>
#include <bit>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <windows.h>

using namespace D2RAccess;
using Radar::Position;

namespace {

// Seen and visited are two rings: standing in a room implies having seen it,
// seeing it implies nothing about standing in it.
void TwoMemories() {
	ExploredRooms::Memory memory;
	Check(!memory.Known(5), "a level with no room seen is unknown");
	Check(memory.Mark(5, 10, 10), "the first sighting of a room is news");
	Check(!memory.Mark(5, 10, 10), "the second is not");
	Check(memory.Explored(5, 10, 10) && !memory.Visited(5, 10, 10), "seen is not visited");
	Check(memory.Visit(5, 10, 10) && memory.Visited(5, 10, 10), "standing in it is, once");
	Check(!memory.Visit(5, 10, 10), "and only once");
	Check(memory.Visit(5, 12, 10) && memory.Explored(5, 12, 10), "visiting an unseen room also sees it");
	Check(memory.Known(5) && !memory.Known(6), "only the level with a room seen is known");
	Check(memory.Count(5) == 2 && memory.VisitedCount(5) == 2, "counts follow both rings");
	memory.Forget();
	Check(memory.Count(5) == 0 && !memory.Known(5), "a new game forgets everything");
}

// What is written is what is read back, level by level, and merging never
// takes a room away.
void RoundTrip() {
	ExploredRooms::Memory memory;
	memory.Mark(2, 4, 4);
	memory.Visit(2, 8, 4);
	memory.Visit(3, 0, 0);
	Check(memory.Dirty(), "changes make the memory dirty");
	const auto all = memory.Serialize();
	Check(memory.Dirty(), "serializing before a successful write keeps changes dirty");
	Check(all == "2 4 4 s\n2 8 4 v\n3 0 0 v\n", "one room per line, seen or visited: " + all);
	Check(memory.Serialize(2) == "2 4 4 s\n2 8 4 v\n", "one level alone can be written");
	ExploredRooms::Memory copy;
	copy.Merge(all);
	Check(copy.Explored(2, 4, 4) && !copy.Visited(2, 4, 4) && copy.Visited(2, 8, 4) && copy.Visited(3, 0, 0),
		"the copy has every room with its own ring");
	Check(!copy.Dirty(), "what was read is not a change to write back");
	copy.Merge("2 4 4 v\nnonsense\n7 1\n");
	Check(copy.Visited(2, 4, 4), "merging upgrades seen to visited");
	Check(copy.Count(7) == 0, "a line that does not parse is skipped whole");
}

std::filesystem::path Directory(const char* name) {
	const auto path = std::filesystem::temp_directory_path() /
		("d2r-access-exploration-" + std::to_string(GetCurrentProcessId())) / name;
	std::filesystem::remove_all(path);
	std::filesystem::create_directories(path);
	return path;
}
void WriteText(const std::filesystem::path& path, const std::string& text) {
	std::ofstream out(path, std::ios::binary);
	out << text;
}
// Distinct generated maps, including layouts of the same level, stay separate.
void Store() {
	const std::vector<NativeLevelExits::LevelRoom> rooms{{{10, 10, 8, 8}, true}, {{18, 10, 8, 8}, false}};
	const std::vector<NativeLevelExits::LevelRoom> reordered{{{18, 10, 8, 8}, false}, {{10, 10, 8, 8}, true}};
	const std::vector<NativeLevelExits::LevelRoom> other{{{10, 10, 8, 8}, true}, {{18, 18, 8, 8}, false}};
	const auto digest = ExploredStore::LayoutDigest(rooms);
	Check(digest == ExploredStore::LayoutDigest(reordered), "the digest does not depend on the game's list order");
	Check(digest != ExploredStore::LayoutDigest(other), "a different layout digests differently");
	const auto directory = Directory("roundtrip");
	const ExploredStore::Key first{0xABCD1234u, 2, digest}, otherLayout{0xABCD1234u, 2, ExploredStore::LayoutDigest(other)};
	ExploredStore::Store::Table table;
	table[first] = {{{10, 10, true}, {18, 10, false}}, 1};
	table[otherLayout] = {{{18, 18, true}}, 2};
	Check(ExploredStore::WriteTable(directory, table), "one archive saves distinct layouts");
	ExploredRooms::Memory loaded;
	Check(ExploredStore::ReadArchive(directory, first, loaded) && loaded.Visited(2, 10, 10)
		&& loaded.Explored(2, 18, 10) && !loaded.Visited(2, 18, 10), "seen and visited survive the binary roundtrip");
	ExploredRooms::Memory separate;
	Check(ExploredStore::ReadArchive(directory, otherLayout, separate) && separate.Visited(2, 18, 18)
		&& !separate.Explored(2, 10, 10), "another layout with the same level id is isolated");
	ExploredStore::Store worker;
	Check(worker.Start(directory), "the store worker starts");
	worker.Submit({{first, {{30, 30, true}}}});
	for (int i = 0; i < 100 && !worker.Ready(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
	Check(worker.Ready() && worker.Load(first, loaded), "a tick loads cached rooms without disk I/O");
	Check(loaded.Visited(2, 30, 30) && loaded.Visited(2, 10, 10),
		"disk records merge with updates submitted during startup");
	worker.Submit({{first, {{20, 20, true}}}});
	worker.Stop();
	ExploredRooms::Memory updated;
	Check(ExploredStore::ReadArchive(directory, first, updated) && updated.Visited(2, 20, 20)
		&& updated.Visited(2, 10, 10) && updated.Visited(2, 30, 30), "shutdown flushes queued updates and keeps imported progress");
	std::filesystem::remove_all(directory);
}
void Migration() {
	const auto directory = Directory("migration");
	const auto valid = directory / "explored-ABCD1234-2-0123456789ABCDEF.txt";
	const auto malformed = directory / "explored-ABCD1234-3-0123456789ABCDEF.txt";
	const auto unrelated = directory / "notes.txt";
	WriteText(valid, "2 10 10 s\n2 20 20 v\n");
	WriteText(malformed, "3 9 9 v\n2 8 8 s\n3 999999999999999999 10 v\n"); // valid row plus mismatched and overflowing rows
	WriteText(unrelated, "keep me");
	bool cleanupIncomplete = false;
	Check(ExploredStore::MigrateDirectory(directory, &cleanupIncomplete) && cleanupIncomplete,
		"valid legacy rows migrate while a partially damaged file is reported and retained");
	ExploredRooms::Memory memory;
	Check(ExploredStore::ReadArchive(directory, {0xABCD1234u, 2, 0x0123456789ABCDEFull}, memory)
		&& memory.Explored(2, 10, 10) && !memory.Visited(2, 10, 10) && memory.Visited(2, 20, 20),
		"migration preserves both room flags under the old map key");
	Check(!std::filesystem::exists(valid) && std::filesystem::exists(malformed) && std::filesystem::exists(unrelated),
		"only a verified imported legacy file is removed");
	ExploredRooms::Memory partial;
	Check(ExploredStore::ReadArchive(directory, {0xABCD1234u, 3, 0x0123456789ABCDEFull}, partial)
		&& partial.Visited(3, 9, 9) && partial.Count(3) == 1,
		"a damaged legacy file contributes its valid row without inventing an overflowed coordinate");
	std::filesystem::remove_all(directory);
}
void FailureAndBounds() {
	const auto directory = Directory("failure");
	ExploredStore::Store::Table table;
	const ExploredStore::Key first{1, 2, 3};
	table[first] = {{{10, 10, true}}, 1};
	Check(ExploredStore::WriteTable(directory, table), "initial archive saves");
	const auto before = std::filesystem::file_size(ExploredStore::ArchiveFile(directory));
	table[{2, 2, 3}] = {{{20, 20, true}}, 2};
	std::filesystem::create_directory(directory / "explored.dat.tmp");
	Check(!ExploredStore::WriteTable(directory, table), "a blocked temporary path fails without replacement");
	ExploredRooms::Memory memory;
	memory.Visit(2, 20, 20);
	(void)memory.Serialize(2);
	Check(memory.Dirty(), "the snapshot remains dirty when persistence fails");
	ExploredRooms::Memory prior;
	Check(std::filesystem::file_size(ExploredStore::ArchiveFile(directory)) == before
		&& ExploredStore::ReadArchive(directory, first, prior) && prior.Visited(2, 10, 10)
		&& !ExploredStore::ReadArchive(directory, {2, 2, 3}, prior), "failed save leaves the last good archive intact");
	std::filesystem::remove(directory / "explored.dat.tmp");
	Check(ExploredStore::WriteTable(directory, table), "the same pending data saves on retry");
	for (uint32_t seed = 3; seed <= 405; ++seed) table[{seed, 2, 3}] = {{{int(seed), 0, false}}, seed};
	Check(ExploredStore::WriteTable(directory, table), "a crowded archive is pruned and saved");
	ExploredStore::Store::Table bounded;
	Check(ExploredStore::ReadTable(directory, bounded) && bounded.size() == ExploredStore::MaxEntries
		&& bounded.find(first) == bounded.end(), "the oldest key goes before the 400-key bound is exceeded");
	const auto goodSize = std::filesystem::file_size(ExploredStore::ArchiveFile(directory));
	bounded[{999, 2, 3}] = {std::vector<ExploredRooms::Room>(ExploredStore::MaxArchiveBytes / 9 + 1), 999};
	Check(!ExploredStore::WriteTable(directory, bounded)
		&& std::filesystem::file_size(ExploredStore::ArchiveFile(directory)) == goodSize,
		"an oversized single map cannot replace the bounded last-good archive");
	const auto legacy = directory / "explored-00000001-2-0000000000000003.txt";
	WriteText(legacy, "2 30 30 v\n");
	WriteText(ExploredStore::ArchiveFile(directory), "truncated");
	Check(!ExploredStore::MigrateDirectory(directory) && std::filesystem::exists(legacy),
		"a malformed archive is never overwritten and legacy progress is retained");
	std::filesystem::remove_all(directory);
}

// The fixed places found on one map are kept with its rooms, in a later game
// too, and never offered on a regenerated one.
void Places() {
	using ExploredStore::Place;
	std::vector<Place> known;
	const Place stairs{9, 0x10002, 102.5, 207, 13, "Al nivel 2 de la Cueva", 4, 3, "To The Cave Level 2"},
		border{3, 0x20003, 300, 400, 3, "Llanuras Frías", 4, 3, "Cold Plains"};
	const Place waypoint{77, 0x50006, 80, 90, 0, "Punto de viaje", 4, 5};
	using ExploredStore::Novelty;
	Check(ExploredStore::Classify(known, stairs, 0) == Novelty::Find, "a place not on the map's list is a find");
	Check(ExploredStore::Classify(known, border, 3) == Novelty::WayBack && ExploredStore::Classify(known, border, 2) == Novelty::Find,
		"the exit back to the level the player came from is not a find");
	Check(ExploredStore::Classify(known, waypoint, 0) == Novelty::Find, "a waypoint, which leads nowhere, is a find");
	Check(ExploredStore::MergePlaces(known, {stairs, border, waypoint}) && known.size() == 3, "new places are added");
	Check(ExploredStore::Classify(known, stairs, 0) == Novelty::Known, "a place already listed, restored or seen, is known");
	Place loaded = stairs;
	loaded.id = 4711, loaded.x = 102;
	Check(!ExploredStore::MergePlaces(known, {loaded}) && known[0] == stairs,
		"the loaded warp at the same place is the same exit, and does not rewrite it");
	Place renamed = stairs;
	renamed.name = "To Cave Level 2";
	Check(ExploredStore::MergePlaces(known, {renamed}) && known[0].name == "To Cave Level 2" && known.size() == 3,
		"a newer name replaces the stored one");
	Place keyed = waypoint;
	keyed.nameKey = "Waypoint";
	Check(ExploredStore::MergePlaces(known, {keyed}) && known[2].nameKey == "Waypoint", "a place found again gains its name key");
	Place nowhere = border, oversized = border;
	nowhere.identity = 0;
	oversized.identity = 0x30004, oversized.name.assign(ExploredStore::MaxPlaceName + 1, 'x');
	Place oversizedKey = border;
	oversizedKey.identity = 0x30005, oversizedKey.nameKey.assign(ExploredStore::MaxPlaceName + 1, 'x');
	Check(!ExploredStore::MergePlaces(known, {nowhere, oversized, oversizedKey}) && known.size() == 3,
		"an unplaceable or oversized place is skipped");
	const Place stone{21, 0x40001, 500, 600, 0, "Piedra de Cairn", 2, 9}, sameStone{22, 0x40001, 500, 600, 0, "Piedra de Cairn", 2, 9},
		secondStone{23, 0x40002, 505, 600, 0, "Piedra de Cairn", 2, 9}, tree{24, 0x40003, 700, 600, 0, "Árbol de Inifuss", 2, 9},
		chest{25, 0x40004, 710, 600, 0, "Piedra de Cairn", 2, 7};
	Check(!ExploredStore::Namesake(known, stone), "the first stone of its name has no namesake");
	std::vector<Place> quests{stone};
	Check(!ExploredStore::Namesake(quests, sameStone), "the same stone seen again is known, not a namesake");
	Check(ExploredStore::Namesake(quests, secondStone), "a second stone of the same name is a namesake");
	Check(!ExploredStore::Namesake(quests, tree), "another quest object is a find of its own");
	Check(!ExploredStore::Namesake(quests, chest), "a namesake shares the category too");
	std::vector<Place> many;
	for (uint64_t i = 1; i <= ExploredStore::MaxPlaces + 5; ++i) many.push_back({1, i, 1, 1, 0, "x"});
	std::vector<Place> bounded;
	ExploredStore::MergePlaces(bounded, many);
	Check(bounded.size() == ExploredStore::MaxPlaces, "a level keeps a bounded number of places");

	const auto directory = Directory("places");
	const ExploredStore::Key map{0xABCD1234u, 9, 0x1111}, regenerated{0xABCD1234u, 9, 0x2222};
	ExploredStore::Store::Table table;
	table[map] = {{{10, 10, true}}, 1, known};
	table[regenerated] = {{{50, 50, true}}, 2};
	Check(ExploredStore::WriteTable(directory, table), "rooms and places save together");
	ExploredRooms::Memory memory;
	std::vector<Place> read;
	Check(ExploredStore::ReadArchive(directory, map, memory, &read) && read == known && memory.Visited(9, 10, 10),
		"places survive the binary roundtrip exactly, kinds, half subtiles and names included");
	read = known;
	Check(ExploredStore::ReadArchive(directory, regenerated, memory, &read) && read.empty(),
		"another layout of the same level has none of them");

	// An archive written before places were kept still reads, and the next save upgrades it.
	std::vector<uint8_t> bytes{'D', '2', 'R', 'E', 'X', 'P', '0', '1'};
	const auto put = [&bytes](uint64_t value, int count) {
		for (int i = 0; i < count; ++i) bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
	};
	put(1, 4); put(0xABCD1234u, 4); put(9, 4); put(0x1111, 8); put(5, 8); put(1, 4); put(20, 4); put(20, 4); put(2, 1);
	uint64_t hash = 14695981039346656037ull;
	for (const uint8_t byte : bytes) { hash ^= byte; hash *= 1099511628211ull; }
	put(hash, 8);
	std::filesystem::remove(ExploredStore::ArchiveFile(directory));
	{
		std::ofstream out(ExploredStore::ArchiveFile(directory), std::ios::binary);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
	ExploredRooms::Memory old;
	read = known;
	Check(ExploredStore::ReadArchive(directory, map, old, &read) && old.Visited(9, 20, 20) && read.empty(),
		"a rooms-only archive loads with no places");

	ExploredStore::Store worker;
	Check(worker.Start(directory), "the store worker starts on the old archive");
	for (int i = 0; i < 100 && !worker.Ready(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
	worker.Submit({{map, {}, {stairs}}});
	ExploredRooms::Memory cached;
	read.clear();
	Check(worker.Ready() && worker.Load(map, cached, &read) && read.size() == 1 && read[0] == stairs && cached.Visited(9, 20, 20),
		"a place found this game is cached with the map's stored rooms");
	worker.Stop();
	std::ifstream in(ExploredStore::ArchiveFile(directory), std::ios::binary);
	char magic[8]{};
	in.read(magic, sizeof magic);
	in.close();
	read.clear();
	Check(std::string(magic, sizeof magic) == "D2REXP03" && ExploredStore::ReadArchive(directory, map, old, &read)
		&& read.size() == 1 && read[0] == stairs, "shutdown writes the places, upgrading the archive");

	// An archive written before name keys were kept reads its places without one.
	bytes = {'D', '2', 'R', 'E', 'X', 'P', '0', '2'};
	put(1, 4); put(0xABCD1234u, 4); put(9, 4); put(0x1111, 8); put(5, 8); put(1, 4); put(20, 4); put(20, 4); put(2, 1);
	const std::string oldName = "Pantano Negro";
	put(1, 2); put(6, 4); put(0x60007, 8); put(std::bit_cast<uint64_t>(15640.0), 8); put(std::bit_cast<uint64_t>(5169.0), 8);
	put(6, 4); put(4, 1); put(3, 1); put(oldName.size(), 1);
	bytes.insert(bytes.end(), oldName.begin(), oldName.end());
	hash = 14695981039346656037ull;
	for (const uint8_t byte : bytes) { hash ^= byte; hash *= 1099511628211ull; }
	put(hash, 8);
	{
		std::ofstream out(ExploredStore::ArchiveFile(directory), std::ios::binary);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
	read.clear();
	Check(ExploredStore::ReadArchive(directory, map, old, &read) && read.size() == 1 && read[0].name == oldName
		&& read[0].nameKey.empty() && read[0].destination == 6, "a version 2 archive loads its places with no name key");
	std::filesystem::remove_all(directory);
}

// A 60x60 open field in one streamed room; the level has one more room to the
// right (world +X) never streamed. Frontier lies to the screen right and down,
// the two quadrants world +X projects into, and nowhere once it is seen.
Radar::World Field(int width, int height) {
	Radar::World world;
	world.walkingMask = Radar::Collision::Wall;
	world.bodyPattern = 0;
	world.rooms.push_back({100, 100, width, height, std::vector<uint16_t>(static_cast<size_t>(width * height), 0)});
	world.player = {100 + width / 2.0, 100 + height / 2.0};
	return world;
}
void Quadrants() {
	Check(Frontier::Quadrant(-1, -1) == Radar::Direction::Up && Frontier::Quadrant(1, 1) == Radar::Direction::Down,
		"world minus/plus both axes is screen up/down");
	Check(Frontier::Quadrant(-1, 1) == Radar::Direction::Left && Frontier::Quadrant(1, -1) == Radar::Direction::Right,
		"the other diagonals are screen left/right");
	constexpr auto Cardinal = static_cast<size_t>(Radar::Frame::Cardinal), Diagonal = static_cast<size_t>(Radar::Frame::Diagonal);
	const auto empty = [](const Frontier::FrameMarks& marks) {
		for (const auto& frame : marks) for (const bool mark : frame) if (mark) return false;
		return true;
	};
	auto world = Field(60, 60);
	const NativeRooms::Bounds east{160, 100, 200, 160}; // the room across the right border, world +X
	auto marks = Frontier::Quadrants(world, {east});
	Check(marks[Cardinal][static_cast<size_t>(Radar::Direction::Right)] && marks[Cardinal][static_cast<size_t>(Radar::Direction::Down)],
		"an unseen room in world +X is frontier to the screen right and down");
	Check(!marks[Cardinal][static_cast<size_t>(Radar::Direction::Up)] && !marks[Cardinal][static_cast<size_t>(Radar::Direction::Left)],
		"and nowhere else");
	for (size_t i = 0; i < Radar::DirectionCount; ++i) {
		const auto& axis = Radar::RaysOf(Radar::Frame::Diagonal)[i].world;
		// The room's corners sit exactly between two rays, so only the two ends of the axis are certain.
		if (axis.x > 0) Check(marks[Diagonal][i], "for the diagonals it is frontier along the ray of world +X");
		if (axis.x < 0) Check(!marks[Diagonal][i], "and never along the opposite ray");
		Check(Frontier::Sector(axis.x, axis.y, Radar::Frame::Diagonal) == i, "each diagonal ray is the sector of its own axis");
	}
	Check(empty(Frontier::Quadrants(world, {})), "no unseen room, no frontier");
	// A wall along the whole right border: the unseen room is there but not reachable.
	for (int y = 0; y < 60; ++y) world.rooms[0].cells[static_cast<size_t>(y * 60 + 59)] = Radar::Collision::Wall;
	Check(empty(Frontier::Quadrants(world, {east})), "frontier behind a wall is not reachable frontier");
	// An unseen room whose rect does not touch the loaded ground is not the edge of it.
	const NativeRooms::Bounds far{400, 400, 440, 440};
	Check(empty(Frontier::Quadrants(Field(60, 60), {far})), "an unseen room nowhere near the loaded ground is no frontier");
}
}

int main() {
	TwoMemories();
	RoundTrip();
	Store();
	Migration();
	FailureAndBounds();
	Places();
	Quadrants();
	Passed();
	return 0;
}
