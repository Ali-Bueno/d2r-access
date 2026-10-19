#include "crossing_memory.h"
#include "test_support.h"

using namespace D2RAccess;

int main() {
	CrossingMemory::Clear();
	constexpr uint64_t Game = 7;
	constexpr int ColdPlains = 3;
	constexpr uint32_t StonyField = 4, BurialGrounds = 17;
	Check(!CrossingMemory::Find(Game, ColdPlains, StonyField, {5700, 5170}), "a border never seen loaded has no crossing");
	CrossingMemory::Note(Game, ColdPlains, {{{5400, 5222}, StonyField}, {{5462, 5480}, BurialGrounds}});
	const auto known = CrossingMemory::Find(Game, ColdPlains, StonyField, {5700, 5170});
	Check(known && known->x == 5400 && known->y == 5222, "the crossing walked across is kept for the game, however far");
	Check(!CrossingMemory::Find(Game, 2, StonyField, {5700, 5170}), "a crossing belongs to the area it was seen from");
	CrossingMemory::Note(Game, ColdPlains, {{{5402, 5223}, StonyField}});
	CrossingMemory::Note(Game, ColdPlains, {{{5300, 5230}, StonyField}});
	const auto nearer = CrossingMemory::Find(Game, ColdPlains, StonyField, {5290, 5200});
	Check(nearer && nearer->x == 5300, "of two openings of a border the one nearer the player is given");
	const auto same = CrossingMemory::Find(Game, ColdPlains, StonyField, {5410, 5220});
	Check(same && same->x == 5400, "a crossing seen again a step away is the same opening");
	Check(!CrossingMemory::Find(Game + 1, ColdPlains, StonyField, {5700, 5170}), "another game is another map");
	CrossingMemory::Note(Game + 1, ColdPlains, {});
	Check(!CrossingMemory::Find(Game, ColdPlains, StonyField, {5700, 5170}), "and the last game's crossings are gone");
	Passed();
	return 0;
}
