#pragma once
#include <cstddef>
#include <cstdint>

// D2R 3.3.93787 random-level-generator layout, recovered offline from the
// generator itself. Evidence and the full derivation: the workspace record
// `re/dungeon-randomization.md`.
//
//   DrlgInit           0x326BA0   SeedRandom(&drlg->rng, mapSeed)
//                                 drlg[+0x840] = mapSeed
//                                 drlg[+0x860] = one draw from it
//   DrlgGetOrAddLevel  0x3267C0   level[+0x1C8] = drlg, level[+0x1F8] = levelId
//                                 SeedRandom(&level[+0x1E4], drlg[+0x860] + levelId)
//                                 its lookup walks drlg[+0x868] then level[+0x1B8],
//                                 comparing level[+0x1F8] -- so those two are the
//                                 act's list of the levels it has instantiated
//   SeedRandom         0x367110   p[0] = seed; p[1] = 666
//
// The Level structure here is the one `EntityLayout::ExtendedLevel` already
// reaches, and `EntityLayout::LevelId` (+0x1F8) is the same field the level
// walk reads -- which is what lets the seed read check itself.
namespace D2RAccess::DrlgLayout {

inline constexpr size_t LevelDrlg = 0x1C8;      // Level -> the act's DRLG
inline constexpr size_t DrlgMapSeed = 0x840;    // the per-character map seed
inline constexpr size_t DrlgInitSeed = 0x860;   // one PRNG draw from it
inline constexpr size_t DrlgFirstLevel = 0x868; // head of the act's level list
inline constexpr size_t LevelNext = 0x1B8;      // the next level in that list

// The game's PRNG: state = state * 0x6AC690C5 + increment, the high half of
// the 64-bit product becoming the next increment. SeedRandom starts the
// increment at 666.
inline constexpr uint64_t PrngMultiplier = 0x6AC690C5ULL;
inline constexpr uint32_t SeedRandomIncrement = 0x29A;

struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
// FNV-1a of the complete functions the offsets above were read from, hashed
// with LayoutCheck::Hash. Recomputed offline against the whole-image dump; the
// same hasher reproduces all of entity_layout.h's entries byte-exact.
inline constexpr Evidence Checks[] = {
	{0x3267C0, 488, 0xD7098129FA6F2A05ULL},  // Level+0x1C8/+0x1E4/+0x1F8, Drlg+0x860/+0x868
	{0x326BA0, 1554, 0x325F41F8CA466A31ULL}, // Drlg+0x840 = map seed, +0x860 = first draw
	{0x367110, 9, 0xDE9CCE43281EC0BDULL},    // SeedRandom: lo = seed, hi = 666
};

}
