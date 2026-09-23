#include "explored_rooms.h"
#include "explored_store.h"
#include "frontier.h"
#include "test_support.h"

#include <filesystem>
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
	auto world = Field(60, 60);
	const NativeRooms::Bounds east{160, 100, 200, 160}; // the room across the right border, world +X
	auto marks = Frontier::Quadrants(world, {east});
	Check(marks[static_cast<size_t>(Radar::Direction::Right)] && marks[static_cast<size_t>(Radar::Direction::Down)],
		"an unseen room in world +X is frontier to the screen right and down");
	Check(!marks[static_cast<size_t>(Radar::Direction::Up)] && !marks[static_cast<size_t>(Radar::Direction::Left)],
		"and nowhere else");
	const auto none = Frontier::Quadrants(world, {});
	Check(!none[0] && !none[1] && !none[2] && !none[3], "no unseen room, no frontier");
	// A wall along the whole right border: the unseen room is there but not reachable.
	for (int y = 0; y < 60; ++y) world.rooms[0].cells[static_cast<size_t>(y * 60 + 59)] = Radar::Collision::Wall;
	const auto walled = Frontier::Quadrants(world, {east});
	Check(!walled[0] && !walled[1] && !walled[2] && !walled[3], "frontier behind a wall is not reachable frontier");
	// An unseen room whose rect does not touch the loaded ground is not the edge of it.
	const NativeRooms::Bounds far{400, 400, 440, 440};
	const auto distant = Frontier::Quadrants(Field(60, 60), {far});
	Check(!distant[0] && !distant[1] && !distant[2] && !distant[3], "an unseen room nowhere near the loaded ground is no frontier");
}
}

int main() {
	TwoMemories();
	RoundTrip();
	Store();
	Migration();
	FailureAndBounds();
	Quadrants();
	Passed();
	return 0;
}
