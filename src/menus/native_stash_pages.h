#pragma once
#include <D2RLPlugin/api.h>
#include <cstdint>
#include <algorithm>

namespace D2RAccess::NativeStashPages {
bool Initialize(const D2RL::PluginContext* context);
// UI owner only. Executes native single-page commands, ignoring the physical
// shortcut's modifiers. The game still owns all page data/binding updates.
bool Change(uintptr_t panel, int direction, int pages);

constexpr int Steps(uint64_t current, uint64_t count, int direction, int pages) {
	if (current >= count || (direction != -1 && direction != 1)
		|| (pages != 1 && pages != 5 && pages != 10)) return 0;
	const uint64_t available = direction < 0 ? current : count - current - 1;
	return static_cast<int>((std::min)(available, static_cast<uint64_t>(pages)));
}
constexpr int Amount(bool shift, bool control) {
	return control ? (shift ? 10 : 5) : (shift ? 1 : 0);
}
}
