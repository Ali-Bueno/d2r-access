#include "pad_filter.h"
#include "test_support.h"

#include <cstdint>

namespace D2RAccess::AsyncLog {
void Info(const D2RL::PluginContext*, const char*) {}
void InfoF(const D2RL::PluginContext*, const char*, ...) {}
void Warn(const D2RL::PluginContext*, const char*) {}
void WarnF(const D2RL::PluginContext*, const char*, ...) {}
}

int main() {
	using namespace D2RAccess::PadFilter;
	constexpr uint16_t A = 0x1000, X = 0x4000, Y = 0x8000, Back = 0x0020, R3 = 0x0080, Up = 0x0001;
	Presses press;

	Check(Filter(A | Up, true, press) == (A | Up), "every other button passes untouched while R3 is claimed");
	Check(Filter(R3 | A, true, press) == A, "a click during play is kept from the game, and only that button");
	Check(Filter(R3, false, press) == 0, "and stays kept while held, even once a menu opens");
	Check(Filter(0, false, press) == 0 && !press.rightThumb.down, "its release ends it");
	Check(Filter(R3, false, press) == R3, "a click in a menu reaches the game");
	Check(Filter(R3, true, press) == R3, "and reaches it whole, even if play resumes while it is held");
	Check(Filter(0, true, press) == 0, "release");
	Check(Filter(R3, true, press) == 0, "the next click during play is kept again");
	Check(Filter(0, true, press) == 0, "release");

	// BACK is the modifier: it always passes, and X or Y pressed with it held
	// during play belongs to the mod's readouts, not to a skill.
	Check(Filter(Back, true, press) == Back, "BACK alone reaches the game");
	Check(Filter(Back | X, true, press) == Back, "BACK+X during play keeps X from the game");
	Check(Filter(X, true, press) == 0, "and X stays kept until it is released, even after BACK is");
	Check(Filter(0, true, press) == 0, "release");
	Check(Filter(X, true, press) == X, "X on its own during play is the game's");
	Check(Filter(X | Back, true, press) == (X | Back), "BACK pressed after X does not take a press already under way");
	Check(Filter(0, true, press) == 0, "release");
	Check(Filter(Back | Y, true, press) == Back, "BACK+Y during play keeps Y from the game");
	Check(Filter(0, true, press) == 0, "release");
	Check(Filter(Back | Y, false, press) == (Back | Y), "in a menu BACK+Y is the game's");
	Check(Filter(0, true, press) == 0, "release");
	constexpr uint16_t ButtonA = 0x1000;
	Check(Filter(Back | ButtonA, true, press) == Back, "BACK+A during play keeps A from the game, so no attack fires");
	Check(Filter(0, true, press) == 0, "release");
	Check(Filter(ButtonA, true, press) == ButtonA, "A on its own during play is the game's");
	Check(Filter(0, true, press) == 0, "release");
	constexpr uint16_t ButtonB = 0x2000;
	Check(Filter(Back | ButtonB, true, press) == Back, "BACK+B during play keeps B from the game");
	Check(Filter(0, true, press) == 0, "release");
	Check(Filter(ButtonB, true, press) == ButtonB, "B on its own during play is the game's");
	Check(Filter(0, true, press) == 0, "release");
	Check(Filter(Back, false, press) == Back, "in a menu BACK is the game's (the character panel's View Advanced Stats)");
	Passed();
}
