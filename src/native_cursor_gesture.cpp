#include "native_cursor_gesture.h"
#include <windows.h>
#include <array>

namespace D2RAccess::NativeCursorGesture {
namespace {
// 0x13E5E0 updates eight cursor objects: 0x1C8 / 0x39 qwords per controller.
constexpr size_t NativeControllerCount = 8;
// 0x234820 separately tracks interpolation and a held directional snap. The
// latter starts ANOTHER snap when released, even after 0x2347B0 resets velocity.
// Both fields are covered by the adapter's complete-function fingerprint.
constexpr size_t InterpolatingOffset = 0x68, DirectionHeldOffset = 0xB0;
struct Cursor { uintptr_t object = 0; bool releasePending = false; };
std::array<Cursor, NativeControllerCount> g_cursors;

bool Cancel(uintptr_t cursor, ResetFn reset) noexcept {
	if (!cursor || !reset) return false;
	__try {
		*reinterpret_cast<uint8_t*>(cursor + InterpolatingOffset) = 0;
		*reinterpret_cast<uint8_t*>(cursor + DirectionHeldOffset) = 0;
		reset(cursor);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
bool Intercept(uintptr_t cursor, bool held, ResetFn reset) noexcept {
	if (!cursor || !reset) return false;
	Cursor* state = nullptr;
	for (auto& entry : g_cursors) {
		if (entry.object == cursor) { state = &entry; break; }
	}
	if (!state && held) {
		for (auto& entry : g_cursors) {
			if (!entry.object) { entry.object = cursor; state = &entry; break; }
		}
	}
	if (!state) return false;
	const bool cancel = held || state->releasePending;
	state->releasePending = held;
	if (!held) *state = {};
	// Consume one final update after release. UI/XInput sampling can precede the
	// game's input dispatch; that dispatch must not restart the consumed gesture.
	if (!cancel) return false;
	if (Cancel(cursor, reset)) return true;
	*state = {}; // a failed native reset must not retain input ownership
	return false;
}
void ResetTracking() { g_cursors = {}; }
}
