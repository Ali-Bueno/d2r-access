#pragma once
#include <cstdint>

namespace D2RAccess::NativeCursorGesture {
using ResetFn = void(*)(uintptr_t);
// Each native controller cursor must consume its release cleanup independently.
// Returns true when this update was handled and stock cursor travel must wait.
bool Intercept(uintptr_t cursor, bool held, ResetFn reset) noexcept;
void ResetTracking();
}
