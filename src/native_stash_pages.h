#pragma once
#include <D2RLPlugin/api.h>
#include <algorithm>
#include <cstdint>

// Shared-stash page commands through the game's own handler (docs/stash-pages.md).
namespace D2RAccess::NativeStashPages {
constexpr int OnePage = 1, TenPages = 10; // the amounts the keys ask for

bool Initialize(const D2RL::PluginContext* context);
// UI owner only. Moves `pages` pages in `direction` (-1 back, +1 forward),
// clamped at the edges, and returns how many moved; -1 when unreadable or refused.
int Change(uintptr_t panel, int direction, int pages);

// How many single steps are possible from `current` of `count` pages.
constexpr int Steps(uint64_t current, uint64_t count, int direction, int pages) {
	if (current >= count || (direction != -1 && direction != 1) || (pages != OnePage && pages != TenPages)) return 0;
	const uint64_t available = direction < 0 ? current : count - current - 1;
	return static_cast<int>((std::min)(available, static_cast<uint64_t>(pages)));
}
}
