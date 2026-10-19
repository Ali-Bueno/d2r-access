#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The player's summons as the game's party portraits show them (a tester,
// 2026-10-14): "Raven: 5, Oak Sage: life 35 percent, Goatman 1: life 65 percent".
// Pure logic, unit-tested. docs/summons-and-corpses-re.md §2
namespace D2RAccess::MinionReport {

struct Pet {
	uint16_t classId = 0;
	int type = -1; // PetType row
	int life = -1; // percent, -1 unknown
};

// A pet type as the portraits show it: IconType::None types (the hireling, traps)
// have no portrait and are left out.
enum class IconType { None = 0, Single = 1, Counted = 2, Each = 3 };
struct Kind {
	IconType icon = IconType::None;
	bool drawHp = false;
	std::string name; // the type's generic name ("Revived")
};

struct Words {
	const char* count;    // %s name, %d count
	const char* life;     // %s name, %d percent
	const char* numbered; // %s name, %d its number among those of that name
};

struct Entry {
	std::string text;
	int pet = -1; // the index in `pets` of the one pet it names, -1 for a count
};
// One entry per counted type, by its pets' own name when they share one (the
// type's name when they do not, as revived monsters), and one per pet of any
// other type, numbered where names repeat; in PetType order, as the portraits.
std::vector<Entry> Entries(const std::vector<Pet>& pets, const std::function<Kind(int type)>& kind,
	const std::function<std::string(uint16_t classId)>& className, const Words& words);


// BACK+RT steps through the mercenary, the summons and the party members, one
// per press (a tester, 2026-10-14), from a list made again at every press. After
// the last it goes round to the first, and a press after RestartMs without one
// starts again at the first: the next look is a new one.
inline constexpr uint64_t RestartMs = 10000;
class Stepper {
public:
	// The entry a press at `now` reads among `count`, -1 when there is none.
	int Next(size_t count, uint64_t now);
private:
	int at_ = -1;
	uint64_t last_ = 0;
};
}
