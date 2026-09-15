#include "interaction_reader.h"
#include "native_tooltips.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "test_support.h"
#include "tab_reader.h"

#include <vector>

namespace {
using namespace D2RAccess;
NativeUi::Focus focus;
NativeUi::Type widgetType;
NativeUi::Cell cell;
UiText::Snapshot inventory, target;
NativeTooltips::Snapshot tooltip;
std::vector<std::string> speech;
bool conversation = false;
uintptr_t tracked = 0;
uint64_t now = 1000, selection = UINT64_MAX;
const D2RL::PluginContext context{};
int inventoryWalks = 0, titleLookups = 0;

bool Tick() { now += 100; return InteractionReader::Tick(&context, now, focus, conversation); }
bool Settle() { Tick(); return Tick(); }
void Tooltip(const std::string& text) {
	tooltip = {};
	tooltip.body = text; tooltip.panel = focus.panel; tooltip.widget = focus.widget; tooltip.selection = selection;
}
void ClearSpeech() { speech.clear(); }
}
namespace D2RAccess::NativeUi {
Type Classify(uintptr_t widget) {
	if (widget == 10) return {"BankPanel"};
	if (widget == 20) return {"PlayerInventoryPanel"};
	if (widget && widget == focus.widget) return widgetType;
	return {};
}
Cell GridCell(uintptr_t) { return cell; }
uint64_t FocusSelection(uintptr_t) { return selection; }
UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t) {
	if (panel == 40) return target;
	++inventoryWalks;
	return inventory;
}
std::string SelectedTabText(uintptr_t widget) {
	for (const auto& node : inventory.nodes) if (node.id == widget) return node.text;
	return {};
}
}
namespace D2RAccess::NativeWidgets {
uintptr_t FindPanel(std::string_view name) { return name == "TooltipsPanel" ? 30 : name == "HUDMonsterHealth" ? 40 : 0; }
std::string ReadName(uintptr_t panel) { return panel == 10 ? "BankExpansionLayout" : "PlayerInventoryExpansionLayout"; }
}
namespace D2RAccess::NativeTooltips {
void Track(uintptr_t panel) { tracked = panel; }
void Reset() { tracked = 0; tooltip = {}; }
Snapshot Latest(uint64_t) { return tracked ? tooltip : Snapshot{}; }
}
namespace D2RAccess::TabReader {
std::string Selected(uintptr_t) {
	std::string result;
	for (const auto& node : inventory.nodes) if (node.kind == UiText::Kind::Tabs) {
		if (!result.empty()) result += ". ";
		result += node.text;
	}
	return result;
}
}
namespace D2RAccess::Localization { std::string Text(std::string_view) { ++titleLookups; return {}; } }
namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool) { speech.emplace_back(text); }
void Stop() {}
}

int main() {
	using namespace D2RAccess;
	Strings::Select(Strings::Language::Spanish);
	Tooltip("Tu alijo personal");
	Check(!Settle() && speech == std::vector<std::string>{"Tu alijo personal"}, "highlighted stash name reads without controller UI focus");
	ClearSpeech(); Settle();
	Check(speech.empty(), "stationary world target does not repeat");
	tooltip = {}; Settle(); Tooltip("Tu alijo personal"); Settle();
	Check(speech == std::vector<std::string>{"Tu alijo personal"}, "returning to the same highlighted object reads again");
	ClearSpeech(); Tooltip("\xEE\x80\x93"); Settle();
	Check(speech.empty(), "a button image is not a world object name");
	Tooltip("Arqueta"); Settle();
	Check(speech == std::vector<std::string>{"Arqueta"}, "different object labels use the same reader");
	ClearSpeech(); conversation = true; Settle();
	Check(speech.empty() && tracked == 0 && InteractionReader::Repeat().empty(), "NPC conversations suspend tooltip capture and clear old world labels");
	conversation = false; Tooltip("Akara"); Settle();
	Check(speech == std::vector<std::string>{"Akara"}, "NPC name can use the native target tooltip");
	ClearSpeech(); tooltip = {};
	target = {40, 0, "HUDMonsterHealth", UiText::Surface::Target, {
		{41, -1, UiText::Kind::Text, "Name", "TextBoxWidget", "Charsi"},
	}};
	Settle();
	Check(speech == std::vector<std::string>{"Charsi"}, "HUD target-name text covers targets without a tooltip");
	ClearSpeech(); focus = {50, 51}; Settle();
	Check(speech.empty(), "another focused menu suppresses background target labels");
	target = {}; focus = {10, 11}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid};
	cell = {0, 0}; selection = 0;
	inventory = {10, 11, "BankExpansionLayout", UiText::Surface::Other, {{12, -1, UiText::Kind::Tabs, "BankTabs", "TabBarWidget", "Personal"}}};
	Tooltip("Poción de curación menor\nRecupera 45 de vida"); tooltip.footer = "Usar";
	Check(Settle() && speech == std::vector<std::string>{"Alijo. Personal. Fila 1, columna 1. Poción de curación menor Recupera 45 de vida. Usar"}, "stash opens with its selected tab, cell, complete item text and footer");
	ClearSpeech(); Settle();
	Check(speech.empty(), "stationary inventory selection stays quiet");
	Check(inventoryWalks == 0 && titleLookups == 1, "localized title is cached; shared tab tracking owns discovery");
	cell = {1, 0}; selection = 1; Tick();
	Tooltip("Poción de curación menor\nRecupera 45 de vida"); tooltip.footer = "Usar";
	Settle();
	Check(speech.size() == 1 && speech[0].starts_with("Fila 1, columna 2. Poción"), "identical items in different cells are announced without repeating the section");
	ClearSpeech(); inventory.nodes[0].text = "Comp."; Settle();
	Check(speech.size() == 1 && speech[0].starts_with("Alijo. Comp."), "changing the actual stash tab announces the new section");
	Check(inventoryWalks == 0 && titleLookups == 1, "cell navigation and selected-tab changes avoid full inventory walks");
	ClearSpeech(); cell = {2, 0}; selection = 2; Settle();
	Check(speech == std::vector<std::string>{"Fila 1, columna 3"}, "old tooltip is discarded when selection moves to a cell without a rendered description");
	ClearSpeech(); focus = {20, 21}; inventory = {}; cell = {0, 1}; selection = uint64_t{1} << 32;
	Tooltip("Pergamino del portal de la ciudad"); Settle();
	Check(speech.size() == 1 && speech[0].starts_with("Inventario. Fila 2, columna 1."), "player inventory uses the same item and grid reader");
	ClearSpeech(); focus.widget = 22; widgetType = {"InventorySlotWidget", UiText::Kind::InventorySlot}; cell = {};
	Tooltip("Hacha de mano\nDaño: 3–6\nDurabilidad: 28 de 28"); Settle();
	Check(speech == std::vector<std::string>{"Inventario. Equipo. Hacha de mano Daño: 3–6 Durabilidad: 28 de 28"}, "equipment announces its subsection and every displayed stat");
	Check(InteractionReader::Repeat() == speech[0], "manual repeat keeps the current complete item readout");
	ClearSpeech(); conversation = true; Settle();
	Check(speech.empty() && InteractionReader::Repeat().empty(), "a dialogue overlay stops inventory readout");
	conversation = false; InteractionReader::Reset();
	Check(tracked == 0, "leaving gameplay or disabling text also disables native tooltip capture");
	focus = {10, 11}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid}; cell = {0, 0}; selection = 0;
	inventory = {10, 11, "BankExpansionLayout", UiText::Surface::Other, {{13, -1, UiText::Kind::Tabs, "BankTabs", "TabBarWidget", "Personal"}}};
	const auto priorWalks = inventoryWalks;
	ClearSpeech(); Tooltip("Poción");
	Check(Tick() && speech.size() == 1 && speech[0].find("Poción") != std::string::npos, "rendered item speech is emitted on the first inventory poll");
	Check(inventoryWalks == priorWalks, "reopening inventory reuses shared tab discovery");
	ClearSpeech(); tooltip = {}; cell = {1, 0}; selection = 1;
	Tick();
	Check(speech.empty(), "new cell permits a brief wait for the renderer");
	now += 33; InteractionReader::Tick(&context, now, focus, false);
	Check(speech == std::vector<std::string>{"Fila 1, columna 2"}, "coordinate navigation speaks by the next 30 Hz poll");
	ClearSpeech();
	InteractionReader::Tick(&context, now + 33, focus, false, "Gemas");
	Check(speech == std::vector<std::string>{"Gemas"}, "switching tabs announces only the changed tab name");
	ClearSpeech(); Settle();
	Check(speech.empty(), "unchanged inventory cell does not interrupt the tab announcement on the next poll");
	focus = {50, 51}; widgetType = {}; cell = {};
	Check(InteractionReader::Tick(&context, now + 100, focus, false, "Árbol de hab.")
		&& speech == std::vector<std::string>{"Árbol de hab."}, "leaving inventory for another tab still reads the new tab");
	ClearSpeech();
	Check(!InteractionReader::Tick(&context, now + 200, focus, true, "Opciones") && speech.empty(), "dialogue priority also suppresses tab speech");
	Passed();
}
