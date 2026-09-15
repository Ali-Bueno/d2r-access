#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Reaches the game's own widget objects, which the SDK never hands out (its
// handles are plugin-owned name paths). D2RLoader's D2RCore.dll resolves the
// game's panel-manager global and its lookup functions into a slot table at
// startup; reading those slots gives the same functions the loader uses
// (re/notes.md, D2RCore 1.2.1-beta call sites: FindPanel at 0x2AE3C2,
// FindChildWidget at 0x2A6FB6, TextBoxWidget::SetText at 0x2A7260).
//
// A widget object holds its strings as the engine's 0x28-byte string type
// {char* data; size_t size; size_t capacityAndFlags; char inline[16]}
// (D2RCore 0x699509 constructs one, 0x314622 destroys one), so a widget's
// name and displayed text can be found by scanning it for such strings.
// Everything here must run on the UI thread and is guarded against bad
// memory and faults in the native calls.
namespace D2RAccess::NativeWidgets {

bool Initialize(const D2RL::PluginContext* context);
bool IsAvailable();

// Native object for a panel or for a child reached by name path; 0 when not found.
uintptr_t FindPanel(std::string_view panel);
uintptr_t FindChild(uintptr_t parent, std::string_view name);
uintptr_t Find(std::string_view panel, std::span<const std::string> path);

struct FoundString {
	size_t      offset = 0;
	std::string value;
};
// Every readable engine string inside the object's first `span` bytes.
std::vector<FoundString> ScanStrings(uintptr_t widget, size_t span);

// Logs the strings found in the widget at panel/path, after `delayMs` so the
// game has filled its runtime text (modal prompts, list rows).
void RequestProbe(std::string_view panel, std::span<const std::string> path, unsigned delayMs);
void Tick(const D2RL::PluginContext* context);

}
