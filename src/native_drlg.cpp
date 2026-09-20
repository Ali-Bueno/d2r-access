#include "native_drlg.h"
#include "drlg_layout.h"
#include "entity_layout.h"
#include "native_memory.h"

namespace D2RAccess::NativeDrlg {
namespace {
using namespace DrlgLayout;
using EntityLayout::ExtendedLevel, EntityLayout::LevelId, EntityLayout::RoomExtended;
using NativeMemory::Peek;
bool g_available = false;
}

void SetAvailable(bool available) { g_available = available; }
bool Available() { return g_available; }

uint32_t DerivedInitSeed(uint32_t mapSeed) {
	return static_cast<uint32_t>(static_cast<uint64_t>(mapSeed) * PrngMultiplier + SeedRandomIncrement);
}

uint32_t MapSeed(uintptr_t activeRoom, int levelId) {
	if (!g_available) return 0;
	const NativeMemory::ReadScope scope;
	uintptr_t extended = 0, level = 0, drlg = 0;
	int32_t   id = 0;
	uint32_t  mapSeed = 0, initSeed = 0;
	if (!activeRoom || !Peek(activeRoom + RoomExtended, extended) || !extended
		|| !Peek(extended + ExtendedLevel, level) || !level) return 0;
	// The level id we already know is the free half of the check: it says this
	// really is the level structure these offsets were read from.
	if (!Peek(level + LevelId, id) || id != levelId) return 0;
	if (!Peek(level + LevelDrlg, drlg) || !drlg) return 0;
	if (!Peek(drlg + DrlgMapSeed, mapSeed) || !Peek(drlg + DrlgInitSeed, initSeed)) return 0;
	// The paid half: the DRLG stores the seed and the first draw taken from
	// it, so the pair has to satisfy the game's own step. Two unrelated fields
	// agree by chance once in 2^32.
	if (!mapSeed || initSeed != DerivedInitSeed(mapSeed)) return 0;
	return mapSeed;
}

}
