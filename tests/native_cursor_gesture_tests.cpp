#include "native_cursor_gesture.h"
#include "test_support.h"
#include <array>
#include <cstddef>
#include <cstring>

namespace {
// Native cursor fixture: fields and extent read by 0x234820 / 0x2347B0.
constexpr size_t Interpolation = 0x68, DirectionHeld = 0xB0;
constexpr size_t Velocity = 0x20, Direction = 0xA0, CursorSize = 0xC8;
using Cursor = std::array<unsigned char, CursorSize>;
int resets = 0;
void StockReset(uintptr_t address) {
	++resets;
	auto* cursor = reinterpret_cast<unsigned char*>(address);
	std::memset(cursor + Velocity, 0, sizeof(uint64_t));
	std::memset(cursor + Direction, 0, sizeof(uint64_t));
	// The stock reset leaves both snap flags untouched.
}
// Direction inactive + previously-held is the release branch in 0x234820.
bool WouldSnapOnRelease(const Cursor& cursor) { return cursor[DirectionHeld] != 0; }
uintptr_t Address(Cursor& cursor) { return reinterpret_cast<uintptr_t>(cursor.data()); }
}
int main() {
	using namespace D2RAccess::NativeCursorGesture;
	Cursor cursor{};
	cursor[Interpolation] = cursor[DirectionHeld] = 1;
	StockReset(Address(cursor));
	Check(WouldSnapOnRelease(cursor), "regression: velocity reset alone leaves a second release-triggered snap armed");
	ResetTracking(); resets = 0;
	Check(!Intercept(Address(cursor), false, StockReset) && resets == 0, "unclaimed cursor input remains untouched");
	Check(Intercept(Address(cursor), true, StockReset), "a claimed D-pad gesture consumes cursor travel");
	Check(cursor[Interpolation] == 0 && !WouldSnapOnRelease(cursor), "both interpolation and the release-triggered snap are cancelled");
	cursor[DirectionHeld] = 1; // native input dispatched again while held
	Check(Intercept(Address(cursor), true, StockReset) && !WouldSnapOnRelease(cursor), "holding cannot rearm the stock release snap");
	cursor[DirectionHeld] = cursor[Interpolation] = 1; // dispatch after the UI sampled release
	Check(Intercept(Address(cursor), false, StockReset), "the release frame still consumes pending native dispatch");
	Check(!WouldSnapOnRelease(cursor) && cursor[Interpolation] == 0, "release cleanup leaves no second movement waiting");
	const auto afterRelease = resets;
	Check(!Intercept(Address(cursor), false, StockReset) && resets == afterRelease, "free cursor updates resume after exactly one release cleanup");
	cursor[DirectionHeld] = 1;
	Check(!Intercept(Address(cursor), false, StockReset) && WouldSnapOnRelease(cursor), "later stock stick gestures retain their native snap behavior");
	Check(Intercept(Address(cursor), true, StockReset), "a second deliberate D-pad gesture can start immediately");
	Check(Intercept(Address(cursor), false, StockReset), "second gesture also cleans up its release");
	std::array<Cursor, 8> controllers{}; // eight slots in the native update loop
	for (auto& controller : controllers) {
		controller[DirectionHeld] = 1;
		Check(Intercept(Address(controller), true, StockReset), "every native controller cursor can own its own cleanup");
	}
	for (auto& controller : controllers) {
		controller[DirectionHeld] = 1;
		Check(Intercept(Address(controller), false, StockReset) && !WouldSnapOnRelease(controller), "one controller's release does not consume another controller's cleanup");
		Check(!Intercept(Address(controller), false, StockReset), "each released controller resumes normally");
	}
	Check(!Intercept(0, true, StockReset), "missing cursor cannot be captured");
	Check(!Intercept(Address(cursor), true, nullptr), "missing native reset cannot be captured");
	Check(!Intercept(1, true, StockReset), "invalid native memory fails open under SEH");
	Check(!Intercept(1, false, StockReset), "failed reset leaves no pending release ownership");
	ResetTracking();
	Passed();
}
