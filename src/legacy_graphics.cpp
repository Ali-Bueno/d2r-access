#include "legacy_graphics.h"

#include "async_log.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <string>

namespace D2RAccess::LegacyGraphics {
namespace {
// Widget types of the layouts: HUDPanel.json and HUDPanelHD.json, CharacterCreatePanel.json and its HD file.
constexpr std::string_view LegacyTypes[] = {"SDHeadsUpPanel", "CharacterCreateWidget"};
constexpr std::string_view ModernTypes[] = {"HUDPanelHD", "HDCharacterSelectorWidget"};
constexpr std::string_view HudPanel = "HUDPanel", CreationPanel = "CharacterCreatePanel";
// Policy: the layouts are loaded again within a frame or two of the message (MEASURED
// 2026-11-11, ResolutionChanged 70-180 ms later); asked again until this long has passed.
constexpr uint64_t SettleMs = 400, GiveUpMs = 2000;
constexpr int CreationDepth = 4; // the heroes stand at most this deep under the creation panel

uint64_t g_changedAt = 0; // 0: nothing to say

std::optional<bool> Under(uintptr_t widget, int depth) {
	if (!widget || !NativeUi::Visible(widget)) return std::nullopt;
	if (const auto mode = OfType(NativeUi::Classify(widget).name)) return mode;
	if (depth <= 0) return std::nullopt;
	for (const uintptr_t child : NativeUi::Children(widget))
		if (const auto mode = Under(child, depth - 1)) return mode;
	return std::nullopt;
}

std::optional<bool> Read() {
	if (const auto mode = Under(NativeWidgets::FindPanel(HudPanel), 0)) return mode;
	return Under(NativeWidgets::FindPanel(CreationPanel), CreationDepth);
}
}

std::optional<bool> OfType(std::string_view type) {
	for (const auto legacy : LegacyTypes) if (type == legacy) return true;
	for (const auto modern : ModernTypes) if (type == modern) return false;
	return std::nullopt;
}

void Changed(uint64_t now) { g_changedAt = now ? now : 1; }

void Tick(const D2RL::PluginContext* context, uint64_t now) {
	if (!g_changedAt || now - g_changedAt < SettleMs) return;
	const auto mode = Read();
	if (!mode && now - g_changedAt < GiveUpMs) return;
	g_changedAt = 0;
	const auto& strings = Strings::Current();
	AsyncLog::InfoF(context, "[legacy] renderer changed: legacy graphics %s", !mode ? "unknown" : *mode ? "on" : "off");
	ScreenReader::Speak(!mode ? strings.legacyGraphicsChanged : *mode ? strings.legacyGraphicsOn : strings.legacyGraphicsOff, true);
}
}
