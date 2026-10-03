#include "radar_ui.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"

#include <windows.h>

#include <algorithm>
#include <string_view>

namespace D2RAccess::RadarUi {
namespace {
constexpr std::string_view MenuSwitcher = "UISwitcher";
// The panels the player works in, by the names the layouts give them. The HUD,
// the mini menu, the automap and the tooltips are open in play and are not here,
// and neither is HireablesPanel: it is the mercenary's portrait on the HUD, open
// for the whole game (MEASURED 2026-09-27: with it here the game was never in play).
constexpr std::string_view Menus[] = {
	"PlayerInventoryExpansionLayout", "PlayerInventoryOriginalLayout", "HirelingInventoryPanel",
	"CharacterStatsPanel", "AdvancedStatsPanel", "SkillsTreePanel", "SkillSelect",
	"BankExpansionLayout", "BankOriginalLayout", "VendorPanelLayout", "HoradricCubeLayout", "ImbueItemsPanel",
	"TradeLayout", "WaypointsPanelExpansion", "WaypointsPanelOriginal", "QuestLogPanelExpansion", "QuestLogPanelOriginal",
	"HireMenuPanel", "PartyPanel", "HelpPanel", "KeyBindingMenu",
	"ChroniclePanel", "LootFilterOptionsPanel", "LootFilterProfilePanel", "LootFilterItemPanel", "LootFilterRuleDetailsPanel",
};
}

bool MenuPanel(std::string_view name) {
	return name.ends_with("Modal") || std::find(std::begin(Menus), std::end(Menus), name) != std::end(Menus);
}

State Inspect(bool foreground) {
	if (!foreground) return State::Inactive;
	if (PanelState::MoviePlaying()) return State::Paused;
	if (!NativeWidgets::SupportsTree()) return State::Unavailable;
	const auto& open = PanelState::OpenPanels();
	for (const auto panel : NativeUi::Panels()) {
		const auto name = NativeWidgets::ReadName(panel);
		// Panels() enumerates loaded panels too. Their widget visibility does
		// not establish that PanelManager has opened them for gameplay.
		if (std::find(open.begin(), open.end(), name) == open.end() || !NativeUi::Visible(panel)) continue;
		// The menu switcher across the top of the menu tree. The game opens and
		// closes it exactly with that tree, so it is the game's own answer to
		// "is a menu up" -- and it does not depend on how a panel classifies.
		// MEASURED 2026-09-20: CharacterStatsPanel classifies as neither Menu nor
		// Narrative, so the directional scan kept answering the right stick while
		// the player was standing in the character panel.
		if (name == MenuSwitcher || MenuPanel(name)) return State::Dialogue;
		const auto surface = NativeUi::Classify(panel).surface;
		if (surface == UiText::Surface::Menu || surface == UiText::Surface::Narrative) return State::Dialogue;
		if (name.starts_with("PauseLayout") || name == "SettingsPanel") return State::Paused;
	}
	return State::Playing;
}

bool InPlay() {
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return Inspect(process == GetCurrentProcessId()) == State::Playing;
}
}
