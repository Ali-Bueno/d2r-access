#pragma once
#include "menu_cursor.h"
#include <optional>

namespace D2RAccess::KeyboardMenuPolicy {
// A connected idle device does not own keyboard menus. If the native mode
// cannot be verified, retain the previous connection-based fallback.
inline bool Automatic(std::optional<bool> controllerMode, bool padConnected) {
	return !controllerMode.value_or(padConnected);
}

// Prefer the panel's content when entering it, keeping all other controls
// available to arrows. A missing content target falls back to geometry.
inline MenuCursor::Place Entry(std::span<const MenuCursor::Target> targets, uintptr_t content = 0) {
	const auto preferred = MenuCursor::Settle(targets, {content, {}, {}});
	return preferred.widget ? preferred : MenuCursor::First(targets);
}
}
