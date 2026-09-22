#pragma once

#include <D2RLPlugin/api.h>
#include <cstdint>
#include <string>
#include <vector>

namespace D2RAccess::NativeTooltips {
struct Snapshot {
	std::string body, footer;
	uintptr_t panel = 0, widget = 0;
	uint64_t selection = UINT64_MAX;
	uint64_t seenAt = 0;
	// The records drawn after it in the same frame: the equipped item a held
	// compare button shows ("Currently Equipped"), and button images. Raw, as
	// copied; the reader decides what is text.
	std::vector<std::string> extras;
};
bool Initialize(const D2RL::PluginContext* context);
// UI thread: choose the live tooltip panel, or zero to suspend capture.
void Track(uintptr_t panel);
void Reset();
Snapshot Latest(uint64_t now);
// Render hook delegates here; this only copies text, never speaks or changes it.
void Capture(uintptr_t tooltip) noexcept;
}
