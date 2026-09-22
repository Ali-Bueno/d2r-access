#pragma once

#include <D2RLPlugin/api.h>
#include <cstdint>
#include <string>

namespace D2RAccess::NativeTooltips {
using DrawPanelFn = void(*)(uintptr_t);
struct Snapshot {
	std::string body, footer;
	uintptr_t panel = 0, widget = 0;
	uint64_t selection = UINT64_MAX;
	uint64_t seenAt = 0;
};
bool Initialize(const D2RL::PluginContext* context);
// UI thread: choose the live tooltip panel, or zero to suspend capture.
void Track(uintptr_t panel);
void Reset();
Snapshot Latest(uint64_t now);
// Render hook delegates here; this only copies text, never speaks or changes it.
void Capture(uintptr_t tooltip) noexcept;
// Panel draw hook delegates here so the delay override can be tested without a live game.
void DrawPanel(uintptr_t panel, DrawPanelFn original);
}
