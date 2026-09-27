#include "radar_ui.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "test_support.h"

using namespace D2RAccess;
namespace {
struct Panel { std::string name; UiText::Surface surface; bool visible = true; };
std::vector<Panel> panels;
bool supported = true;
}
namespace D2RAccess::NativeUi {
std::vector<uintptr_t> Panels() {
	std::vector<uintptr_t> result;
	for (size_t i = 0; i < panels.size(); ++i) result.push_back(i + 1);
	return result;
}
bool Visible(uintptr_t panel) { return panels.at(panel - 1).visible; }
Type Classify(uintptr_t panel) { return {.surface = panels.at(panel - 1).surface}; }
}
namespace D2RAccess::NativeWidgets {
bool SupportsTree() { return supported; }
std::string ReadName(uintptr_t panel) { return panels.at(panel - 1).name; }
}
int main() {
	using State = RadarUi::State;
	using Surface = UiText::Surface;
	Check(RadarUi::Inspect(false) == State::Inactive, "inactive window has its own suspension reason");
	supported = false;
	Check(RadarUi::Inspect(true) == State::Unavailable, "missing UI reader is distinct from a pause");
	supported = true;
	panels = {{"PauseLayoutGarden", Surface::Other}, {"SettingsPanel", Surface::Other},
		{"NpcDialogPanel", Surface::Menu}, {"ScrollSpeechPanel", Surface::Narrative},
		{"PlayerInventoryExpansionLayout", Surface::Other}, {"AutoMap", Surface::Other}};
	Check(RadarUi::Inspect(true) == State::Playing, "preloaded visible pause and dialogue roots do not suspend gameplay");
	PanelState::Opened("PauseLayoutGarden");
	Check(RadarUi::Inspect(true) == State::Paused, "opening the pause menu suspends cues");
	PanelState::Closed("PauseLayoutGarden");
	Check(RadarUi::Inspect(true) == State::Playing, "closing pause resumes despite a retained native widget");
	PanelState::Opened("SettingsPanel");
	Check(RadarUi::Inspect(true) == State::Paused, "open settings suspend cues");
	panels[1].visible = false;
	Check(RadarUi::Inspect(true) == State::Playing, "a hidden closing panel does not prolong suspension");
	PanelState::Reset();
	PanelState::Opened("AutoMap");
	Check(RadarUi::Inspect(true) == State::Playing, "the automap keeps unpaused sensing active");
	// In keyboard mode the game opens no menu switcher with a menu.
	PanelState::Opened("PlayerInventoryExpansionLayout");
	Check(RadarUi::Inspect(true) == State::Dialogue, "an open inventory is a menu with no menu switcher beside it");
	PanelState::Closed("PlayerInventoryExpansionLayout");
	Check(RadarUi::Inspect(true) == State::Playing, "closing it resumes gameplay");
	Check(RadarUi::MenuPanel("DropGoldModal") && RadarUi::MenuPanel("SkillsTreePanel"), "a modal and the skill tree are menus");
	Check(!RadarUi::MenuPanel("MiniMenuPanel") && !RadarUi::MenuPanel("HUDPanel"), "the mini menu and the HUD are not");
	Check(!RadarUi::MenuPanel("HireablesPanel") && RadarUi::MenuPanel("HireMenuPanel"),
		"the mercenary's portrait is open for the whole game and is not a menu; the list of mercenaries to hire is");
	PanelState::Opened("NpcDialogPanel");
	Check(RadarUi::Inspect(true) == State::Dialogue, "active NPC dialogue has a separate suspension reason");
	PanelState::Closed("NpcDialogPanel");
	Check(RadarUi::Inspect(true) == State::Playing, "closing an NPC menu resumes gameplay");
	PanelState::Opened("ScrollSpeechPanel");
	Check(RadarUi::Inspect(true) == State::Dialogue, "open narrative speech suspends cues");
	PanelState::Closed("ScrollSpeechPanel");
	Check(RadarUi::Inspect(true) == State::Playing, "ending narrative speech resumes gameplay");
	// MEASURED 2026-09-27, 12:19 to 12:21: every panel the game opened while the
	// character stood in the camp with no menu up, the line of chat and the
	// automap among them. With one of them taken for a menu the arrows do not walk.
	PanelState::Reset();
	panels.clear();
	for (const char* name : {"LoadScreenPanel", "PanelBordersPanel", "TooltipsPanel", "HUDWarnings", "HUDPanel", "HUDMonsterHealth",
		"HUDLevelNames", "InGameMoviePlayerPanel", "HireablesPanel", "HUDMessagePanel", "MiniMenuPanel", "HelpButtonPanel",
		"ShowItemsPanel", "ChatPanel", "MessageLogPanel_800x600", "CandidateListPanel", "AutoMap"}) {
		const std::string_view panel = name;
		panels.push_back({name, panel == "HUDMessagePanel" ? Surface::NotificationFeed : panel == "HUDMonsterHealth" ? Surface::Target
			: panel == "HUDLevelNames" ? Surface::Area : Surface::Other});
		PanelState::Opened(name);
		Check(!RadarUi::MenuPanel(name), name);
	}
	Check(RadarUi::Inspect(true) == State::Playing, "with every panel of play open and no menu up the game is in play");
	Passed();
}
