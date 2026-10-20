#include "test_support.h"
#include <array>
#include "../src/menus/native_stash_pages.cpp"

// Production native adapter with only the actual engine command/getters
// replaced. No game, window or input is controlled by this fixture.
namespace {
std::array<uint64_t,64> bank{};
uint64_t total = 30;
uint8_t previous = 0;
bool physicalShift = false, physicalControl = false, ignoreSingle = false;
int calls = 0;
uint64_t& Current() { return bank[(0x168 + previous*8)/8]; }
uint64_t Count(uintptr_t) { return total; }
uint8_t Season(uintptr_t) { return previous; }
uint64_t Page(uintptr_t, bool forward, bool single) {
	++calls;
	const uint64_t amount = single && !ignoreSingle ? 1 : physicalControl ? 10 : physicalShift ? 5 : 1;
	Current() = forward ? (std::min)(total-1, Current()+amount) : Current() >= amount ? Current()-amount : 0;
	return Current();
}
}
int main() {
	using namespace D2RAccess::NativeStashPages;
	g_page = Page; g_count = Count; g_season = Season;
	const auto panel = reinterpret_cast<uintptr_t>(bank.data());
	Check(Amount(true,false) == 1 && Amount(false,true) == 5 && Amount(true,true) == 10 && Amount(false,false) == 0,
		"requested Shift/Ctrl/Ctrl+Shift amounts are 1/5/10");
	for (const int pages : {1,5,10}) for (const bool shift : {false,true}) for (const bool control : {false,true}) {
		physicalShift = shift; physicalControl = control; Current() = 10; calls = 0;
		Check(Change(panel,1,pages) && Current() == static_cast<uint64_t>(10+pages) && calls == pages,
			"native production adapter moves exact amount despite physical modifiers");
		Check(Change(panel,-1,pages) && Current() == 10 && calls == pages*2, "previous returns exact amount");
	}
	total = 5; Current() = 3; calls = 0;
	Check(Change(panel,1,10) && Current() == 4 && calls == 1, "last-page edge clamps instead of wrapping");
	Check(Change(panel,1,10) && calls == 1, "already at last page emits no command");
	Check(Change(panel,-1,10) && Current() == 0 && calls == 5, "first-page edge also clamps");
	Check(Change(panel,-1,1) && calls == 5, "already at first page emits no command");
	previous = 1; bank[0x168/8] = 2; Current() = 0;
	Check(Change(panel,1,1) && Current() == 1 && bank[0x168/8] == 2, "previous-season page uses its separate native field");
	previous = 2; Check(!Change(panel,1,1), "unfamiliar season cannot select arbitrary memory");
	previous = 0; Current() = 5;
	Check(!Change(panel,1,1), "invalid page fails closed");
	Current() = 0; physicalShift = true; physicalControl = false; total = 30; ignoreSingle = true;
	Check(!Change(panel,1,1), "unexpected native multi-page result is detected");
	Check(!Change(panel,0,1) && !Change(panel,1,2) && !Change(0,1,1), "invalid command/amount/context is refused");
	Passed();
}
