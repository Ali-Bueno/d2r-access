#include "native_drlg.h"
#include "drlg_layout.h"
#include "entity_layout.h"
#include "test_support.h"
#include <array>
#include <cstring>

using namespace D2RAccess;
using namespace DrlgLayout;
using namespace EntityLayout;
namespace {
template <size_t N> struct Memory {
	std::array<uint8_t, N> data{};
	uintptr_t              Address() const { return reinterpret_cast<uintptr_t>(data.data()); }
	template <class T> void At(size_t offset, T value) { std::memcpy(data.data() + offset, &value, sizeof(value)); }
};
constexpr uint32_t Seed = 0x6AF97A27;
constexpr int      LevelNumber = 9;

// The three structures the read walks: the player's room, its extended room,
// the level, and the act's DRLG.
struct Fixture {
	Memory<0x40>  room;
	Memory<0x100> extended;
	Memory<0x200> level;
	Memory<0x900> drlg;
	Fixture() {
		room.At(RoomExtended, extended.Address());
		extended.At(ExtendedLevel, level.Address());
		level.At<int32_t>(LevelId, LevelNumber);
		level.At(LevelDrlg, drlg.Address());
		drlg.At<uint32_t>(DrlgMapSeed, Seed);
		drlg.At<uint32_t>(DrlgInitSeed, NativeDrlg::DerivedInitSeed(Seed));
	}
};
}

int main() {
	NativeDrlg::SetAvailable(false);
	Fixture blocked;
	Check(NativeDrlg::MapSeed(blocked.room.Address(), LevelNumber) == 0, "nothing is read until the image fingerprints are verified");

	NativeDrlg::SetAvailable(true);
	{
		Fixture fixture;
		Check(NativeDrlg::MapSeed(fixture.room.Address(), LevelNumber) == Seed, "the map seed is read through the room the world reader already has");
	}
	{
		// The game derives the init seed from the map seed with its own PRNG
		// step; unrelated fields would not agree.
		Fixture fixture;
		fixture.drlg.At<uint32_t>(DrlgInitSeed, NativeDrlg::DerivedInitSeed(Seed) + 1);
		Check(NativeDrlg::MapSeed(fixture.room.Address(), LevelNumber) == 0, "a pair that fails the game's own derivation is not reported");
	}
	{
		Fixture fixture;
		Check(NativeDrlg::MapSeed(fixture.room.Address(), LevelNumber + 1) == 0, "a level id that does not match means these are not the expected structures");
	}
	{
		Fixture fixture;
		fixture.drlg.At<uint32_t>(DrlgMapSeed, 0u);
		fixture.drlg.At<uint32_t>(DrlgInitSeed, NativeDrlg::DerivedInitSeed(0));
		Check(NativeDrlg::MapSeed(fixture.room.Address(), LevelNumber) == 0, "a zero seed is reported as unknown, not as a seed of zero");
	}
	{
		Fixture fixture;
		fixture.level.At<uintptr_t>(LevelDrlg, 0);
		Check(NativeDrlg::MapSeed(fixture.room.Address(), LevelNumber) == 0, "a level with no DRLG yields nothing");
	}
	Check(NativeDrlg::MapSeed(0, LevelNumber) == 0, "no room, no seed");

	// SeedRandom stores {seed, 666} and one draw is state * 0x6AC690C5 + hi.
	Check(NativeDrlg::DerivedInitSeed(Seed) == static_cast<uint32_t>(static_cast<uint64_t>(Seed) * 0x6AC690C5ULL + 0x29A),
		"the derivation is the game's own PRNG step");
	Check(NativeDrlg::DerivedInitSeed(1) != NativeDrlg::DerivedInitSeed(2), "different seeds derive differently");
	Passed();
}
