#include "test_support.h"
#include <array>
#include "../src/native_stash_pages.cpp"

// The production adapter with only the engine command and getters replaced.
namespace {
std::array<uint64_t, 64> bank{};
uint64_t total = 30;
uint8_t season = 0;
int calls = 0;
bool multiPage = false;
uint64_t& Current() { return bank[(0x168 + season * 8) / 8]; }
uint64_t Count(uintptr_t) { return total; }
uint8_t Season(uintptr_t) { return season; }
uint64_t Page(uintptr_t, bool forward, bool) {
	++calls;
	const uint64_t amount = multiPage ? 5 : 1;
	Current() = forward ? (std::min)(total - 1, Current() + amount) : Current() >= amount ? Current() - amount : 0;
	return Current();
}
}
int main() {
	using namespace D2RAccess::NativeStashPages;
	Check(Steps(10, 30, 1, 10) == 10 && Steps(10, 30, -1, 10) == 10 && Steps(10, 30, 1, 1) == 1, "steps by 1 or 10");
	Check(Steps(25, 30, 1, 10) == 4 && Steps(3, 30, -1, 10) == 3, "steps clamp at the edges");
	Check(Steps(29, 30, 1, 1) == 0 && Steps(0, 30, -1, 1) == 0, "no step past either end");
	Check(Steps(30, 30, 1, 1) == 0 && Steps(0, 0, 1, 1) == 0, "an invalid current page moves nothing");
	Check(Steps(1, 30, 0, 1) == 0 && Steps(1, 30, 1, 5) == 0 && Steps(1, 30, 1, 0) == 0, "only +-1 and 1 or 10 pages");
	Check(Steps(0, 1000, 1, 10) == 10 && Steps(995, 1000, 1, 10) == 4, "a 1000-page stash");

	g_page = Page; g_count = Count; g_season = Season;
	const auto panel = reinterpret_cast<uintptr_t>(bank.data());
	Current() = 10; calls = 0;
	Check(Change(panel, 1, 10) == 10 && Current() == 20 && calls == 10, "ten pages forward, one command each");
	Check(Change(panel, -1, 10) == 10 && Current() == 10, "ten pages back");
	Check(Change(panel, 1, 1) == 1 && Current() == 11, "one page forward");
	total = 5; Current() = 3; calls = 0;
	Check(Change(panel, 1, 10) == 1 && Current() == 4 && calls == 1, "last page clamps instead of wrapping");
	Check(Change(panel, 1, 1) == 0 && calls == 1, "already on the last page: no command");
	Check(Change(panel, -1, 10) == 4 && Current() == 0 && calls == 5, "first page clamps");
	Check(Change(panel, -1, 1) == 0 && calls == 5, "already on the first page: no command");
	season = 1; bank[0x168 / 8] = 2; Current() = 0;
	Check(Change(panel, 1, 1) == 1 && Current() == 1 && bank[0x168 / 8] == 2, "previous season uses its own field");
	season = 2; Check(Change(panel, 1, 1) == -1, "an unfamiliar season cannot select memory");
	season = 0; total = 30; Current() = 40;
	Check(Change(panel, 1, 1) == -1, "an invalid current page fails closed");
	Current() = 0; multiPage = true;
	Check(Change(panel, 1, 1) == -1, "an unexpected multi-page result is detected");
	multiPage = false;
	Check(Change(panel, 0, 1) == -1 && Change(panel, 1, 5) == -1 && Change(0, 1, 1) == -1, "invalid direction, amount, panel refused");
	g_page = nullptr;
	Check(Change(panel, 1, 1) == -1, "without the verified command nothing is called");
	Passed();
}
