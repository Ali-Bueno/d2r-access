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
	constexpr uint16_t A = 0x1000, R3 = 0x0080, Up = 0x0001;
	Press press;

	Check(Filter(A | Up, true, press) == (A | Up), "every other button passes untouched while R3 is claimed");
	Check(Filter(R3 | A, true, press) == A, "a click during play is kept from the game, and only that button");
	Check(Filter(R3, false, press) == 0, "and stays kept while held, even once a menu opens");
	Check(Filter(0, false, press) == 0 && !press.down, "its release ends it");
	Check(Filter(R3, false, press) == R3, "a click in a menu reaches the game");
	Check(Filter(R3, true, press) == R3, "and reaches it whole, even if play resumes while it is held");
	Check(Filter(0, true, press) == 0, "release");
	Check(Filter(R3, true, press) == 0, "the next click during play is kept again");
	Passed();
}
