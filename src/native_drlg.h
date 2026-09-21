#pragma once
#include <cstdint>

// The map seed: the number D2R generates every random level from.
//
// A level's layout is a pure function of (map seed, level id). The generator
// seeds a per-level RNG with `drlg->initSeed + levelId` and draws every room
// rectangle from it, so two visits with the same seed produce the same rooms.
// The seed itself lives in the character's save file and never changes, which
// is why a dungeon is the same dungeon for that character in every game. See
// the workspace record `re/dungeon-randomization.md` for the derivation.
//
// Read-only, and it checks itself: the DRLG keeps both the raw seed and the
// first draw taken from it, and those two must satisfy the game's own PRNG
// step. A structure that fails that identity is not the one these offsets
// describe, so nothing is reported.
namespace D2RAccess::NativeDrlg {

// Enabled once LayoutCheck::Verify has passed drlg_layout.h's Checks against
// the running image. Until then MapSeed reports nothing: offsets recovered
// from functions that are no longer there describe some other structure.
void SetAvailable(bool available);
bool Available();

// 0 when the seed is unavailable -- no room, a level id that does not match,
// or a structure that fails its own check. Never throws; `activeRoom` is the
// player's Room1, the same pointer the room reader takes.
uint32_t MapSeed(uintptr_t activeRoom, int levelId);

// The game's own derivation, exposed for the unit tests: SeedRandom() stores
// {seed, 666} and one draw is state * 0x6AC690C5 + increment.
uint32_t DerivedInitSeed(uint32_t mapSeed);

}
