#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <optional>
#include <string_view>

// Legacy graphics, switched by the game's Legacy Toggle key or the creation screen's button:
// the mode is told from the layouts the game has loaded, and said when it changes.
// docs/legacy-graphics.md
namespace D2RAccess::LegacyGraphics {
// Pure. The mode a widget's type belongs to, nothing for a type both modes share: the HUD's
// root (HUDPanel.json, HUDPanelHD.json) and the creation screen's heroes.
std::optional<bool> OfType(std::string_view type);
// UI thread: the game says its renderer changed (SystemMessage:RenderingToHDChanged).
void Changed(uint64_t now);
// UI thread: says the new mode once the layouts are loaded again.
void Tick(const D2RL::PluginContext* context, uint64_t now);
}
