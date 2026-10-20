#include "test_support.h"
#include "../src/input/native_keyboard.cpp"

namespace {
int originals = 0, claims = 0;
bool typing = false;
uintptr_t seenBinding = 0, seenTable = 0;
bool seenForced = false;
bool Original(uintptr_t binding, uintptr_t table, bool forced) {
	++originals; seenBinding = binding; seenTable = table; seenForced = forced; return false;
}
bool Claim(uint16_t key) noexcept { ++claims; return !typing && key == 'A'; }
struct Binding { uint32_t action; uint16_t key; uint16_t padding = 0; };
}
int main() {
	using namespace D2RAccess::NativeKeyboard;
	g_original = Original; g_redirect = Claim;
	Binding binding{0,'A'};
	const auto address = reinterpret_cast<uintptr_t>(&binding);
	Check(Observe(address,123,false) && originals == 0 && claims == 1, "A is claimed BEFORE native Character dispatch");
	Check(TakeClaims() == 1 && TakeClaims() == 0, "UI diagnostics drain claims without logging in the native hook");
	binding.key = 'C';
	Check(!Observe(address,123,false) && originals == 1 && claims == 2, "native C is preserved");
	binding.key = 'A'; typing = true;
	Check(!Observe(address,123,false) && originals == 2, "text-owned A reaches original path");
	typing = false; const int before = claims;
	Check(!Observe(address,456,true) && originals == 3 && claims == before && seenForced && seenTable == 456,
		"forced controller event keeps original arguments and never asks keyboard owner");
	binding.action = 44;
	Check(!Observe(address,789,false) && originals == 4 && claims == before, "unrelated native action is preserved");
	Check(!Observe(1,999,false) && originals == 5 && seenBinding == 1 && seenTable == 999, "unreadable record fails open unchanged");
	Reset(); binding.action = 0;
	Check(!Observe(address,123,false) && originals == 6 && claims == before, "unload/reset retires callback before dispatcher");
	Passed();
}
