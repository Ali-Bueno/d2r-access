#include "interaction_reader.h"
#include "native_tooltips.h"
#include "native_progression.h"
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
UiText::Snapshot buttonSnapshot;
NativeTooltips::Snapshot tooltip;
NativeProgression::Details progression;
std::vector<std::string> speech;
bool conversation = false;
uintptr_t tracked = 0;
uint64_t now = 1000, selection = UINT64_MAX;
const D2RL::PluginContext context{};
int inventoryWalks = 0, titleLookups = 0;
int equipmentLocation = 0;
NativeUi::ItemPresence itemPresence = NativeUi::ItemPresence::Unknown;

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
ItemPresence GridItemPresence(uintptr_t) { return itemPresence; }
int EquipmentLocation(uintptr_t) { return equipmentLocation; }
uint64_t FocusSelection(uintptr_t) { return selection; }
UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t) {
	if (panel == 40) return target;
	if (panel == focus.widget && (widgetType.kind == UiText::Kind::Button || widgetType.kind == UiText::Kind::StatButton || widgetType.kind == UiText::Kind::TabButton)) return buttonSnapshot;
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
namespace D2RAccess::NativeProgression {
Details Read(NativeUi::Focus, const NativeUi::Type&) { return progression; }
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
	Check(Settle() && speech == std::vector<std::string>{"Alijo. Personal. Poción de curación menor Recupera 45 de vida. Usar"}, "stash opens with its selected tab and complete item text, without coordinates");
	ClearSpeech(); Settle();
	Check(speech.empty(), "stationary inventory selection stays quiet");
	Check(inventoryWalks == 0 && titleLookups == 1, "localized title is cached; shared tab tracking owns discovery");
	cell = {1, 0}; selection = 1; Tick();
	Tooltip("Poción de curación menor\nRecupera 45 de vida"); tooltip.footer = "Usar";
	Settle();
	Check(speech == std::vector<std::string>{"Poción de curación menor Recupera 45 de vida. Usar"}, "identical items in different cells are announced without coordinates or section repetition");
	ClearSpeech(); inventory.nodes[0].text = "Comp."; Settle();
	Check(speech.size() == 1 && speech[0].starts_with("Alijo. Comp."), "changing the actual stash tab announces the new section");
	Check(inventoryWalks == 0 && titleLookups == 1, "cell navigation and selected-tab changes avoid full inventory walks");
	ClearSpeech(); cell = {2, 0}; selection = 2; Settle();
	Check(speech == std::vector<std::string>{"Fila 1, columna 3"}, "old tooltip is discarded when selection moves to a cell without a rendered description");
	ClearSpeech(); focus = {20, 21}; inventory = {}; cell = {0, 1}; selection = uint64_t{1} << 32;
	Tooltip("Pergamino del portal de la ciudad"); Settle();
	Check(speech == std::vector<std::string>{"Inventario. Pergamino del portal de la ciudad"}, "player inventory reads the item without coordinates");
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
	conversation = false; InteractionReader::Reset(); inventory = {}; tooltip = {}; ClearSpeech();
	focus = {20, 22}; widgetType = {"InventorySlotWidget", UiText::Kind::InventorySlot}; equipmentLocation = 1;
	Check(InteractionReader::HandlesFocus(focus), "native equipment focus remains owned by the interaction reader");
	Settle();
	Check(speech == std::vector<std::string>{"Inventario. Equipo. Cabeza"}, "equipment location is named even without an item tooltip");
	ClearSpeech(); focus.widget = 23; equipmentLocation = 2; Settle();
	Check(speech == std::vector<std::string>{"Cuello"}, "moving between equipment slots reads the location without repeating the group");
	ClearSpeech(); equipmentLocation = 4; focus.widget = 24; Tooltip("Hacha de mano"); Settle();
	Check(speech == std::vector<std::string>{"Arma derecha. Hacha de mano"}, "equipment location precedes its rendered item description");
	ClearSpeech(); tooltip = {}; equipmentLocation = 999; focus.widget = 25; Settle();
	Check(speech.empty(), "unknown equipment locations do not invent a slot name");
	equipmentLocation = 0; tooltip = {}; focus.widget = 26; widgetType = {"ButtonWidget", UiText::Kind::Button};
	buttonSnapshot = {26, 26, "gold_button", UiText::Surface::Other, {{26, -1, UiText::Kind::Button, "gold_button", "ButtonWidget", "Soltar oro"}}};
	Check(InteractionReader::HandlesFocus(focus), "icon buttons use native focus instead of rectangle matching");
	ClearSpeech(); Check(Tick() && speech == std::vector<std::string>{"Soltar oro"}, "gold action is announced immediately upon leaving equipment");
	ClearSpeech(); Tooltip("Soltar oro"); Settle();
	Check(speech.empty(), "matching rendered tooltip does not repeat the button label");
	focus.widget = 27; buttonSnapshot = {27, 27, "AnyScrollAction", UiText::Surface::Other, {{27, -1, UiText::Kind::Button, "AnyScrollAction", "ButtonWidget", ""}}};
	Tooltip("Pergamino del portal de la ciudad"); tooltip.footer = "Usar";
	Check(Tick() && speech == std::vector<std::string>{"Pergamino del portal de la ciudad. Usar"}, "focused action can read its complete rendered help without a native caption");
	ClearSpeech(); focus.widget = 28; buttonSnapshot = {28, 28, "OtherAction", UiText::Surface::Other, {{28, -1, UiText::Kind::Button, "OtherAction", "ButtonWidget", "Identificar"}}};
	Check(Tick() && speech == std::vector<std::string>{"Identificar"}, "previous action tooltip is rejected after focus changes");
	ClearSpeech(); widgetType = {"ToggleButtonWidget", UiText::Kind::Toggle};
	Check(!InteractionReader::HandlesFocus(focus) && !Tick() && speech.empty(), "toggle state stays with the existing value reader");
	widgetType = {"ButtonWidget", UiText::Kind::Button}; conversation = true; ClearSpeech();
	Check(!Tick() && speech.empty(), "conversation priority suppresses background action help");
	conversation = false; InteractionReader::Reset(); inventory = {}; ClearSpeech();
	focus = {20, 21}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid}; cell = {0, 0}; selection = 0;
	itemPresence = NativeUi::ItemPresence::Occupied;
	for (int i = 0; i < 5; ++i) Tick();
	Check(speech.empty(), "occupied cell stays quiet while the stock tooltip is delayed, without early coordinates");
	Tooltip("Hacha grande\nDaño: 6-13"); tooltip.footer = "Coste: 331";
	Check(Tick() && speech == std::vector<std::string>{"Inventario. Hacha grande Daño: 6-13. Coste: 331"}, "delayed item reads immediately with all properties and price but no coordinates");
	ClearSpeech(); selection = 1; cell = {1, 0}; Settle();
	Check(speech.empty(), "moving to another occupied cell waits instead of reading stale item text or coordinates");
	Tooltip("Hacha de mano"); Tick();
	Check(speech == std::vector<std::string>{"Hacha de mano"}, "next occupied cell announces only the item");
	Check(InteractionReader::Repeat() == "Inventario. Hacha de mano", "repeat also omits coordinates for a described item");
	ClearSpeech(); selection = 2; cell = {2, 0}; itemPresence = NativeUi::ItemPresence::Empty; Settle();
	Check(speech == std::vector<std::string>{"Fila 1, columna 3"}, "confirmed empty cell retains quick coordinate feedback after an occupied cell");
	ClearSpeech(); itemPresence = NativeUi::ItemPresence::Occupied; Tooltip("Poción"); Tick();
	Check(speech == std::vector<std::string>{"Poción"}, "an item appearing in the same cell replaces its coordinate readout");
	conversation = false; InteractionReader::Reset(); ClearSpeech();
	focus = {60, 61}; widgetType = {"CharacterStatLevelUpButtonWidget", UiText::Kind::StatButton};
	buttonSnapshot = {61, 61, "StrengthPointButton", UiText::Surface::Other, {{61, -1, UiText::Kind::StatButton, "StrengthPointButton", "", "Gastar punto"}}};
	progression.readout = {"Stats", "Personaje", "Fuerza. 30. Gastar punto", "61"};
	progression.value = "30"; progression.points = "5 puntos";
	Check(InteractionReader::HandlesFocus(focus) && Tick() && speech == std::vector<std::string>{"Personaje. Fuerza. 30. Gastar punto"}, "stat selection reads its own label and value immediately");
	ClearSpeech(); Tooltip("Gastar punto"); Settle();
	Check(speech.empty(), "rendered stat action does not repeat generic spend instruction");
	buttonSnapshot.nodes[0].enabled = false; Tick();
	Check(speech.size() == 1 && speech[0].ends_with("no disponible"), "enriched stat readout preserves disabled state");
	ClearSpeech(); progression.value = "31"; progression.points = "4 puntos";
	progression.readout.option = "Fuerza. 31. 4 puntos. Gastar punto"; Tick();
	Check(speech == std::vector<std::string>{"31. 4 puntos"}, "spending a stat point reads only the changed value and remaining points");
	InteractionReader::Reset(); ClearSpeech(); progression = {}; tooltip = {};
	focus = {70, 71}; widgetType = {"ButtonWidget", UiText::Kind::Button}; buttonSnapshot = {};
	progression.skill = true; progression.points = "1 punto";
	progression.readout = {"Skills", "Gritos de guerra", "Fila 1, columna 1", "71"};
	Check(Tick() && speech.empty(), "skill awaits its matching rendered description without speaking a blank button or coordinates");
	Tooltip("Huyen aterrorizados.\n\xEE\x81\xBE" "2Aullido\nSin aprender"); Tick();
	Check(speech == std::vector<std::string>{"Gritos de guerra. Aullido. Sin aprender Huyen aterrorizados. Fila 1, columna 1. 1 punto"}, "skill description is reordered and includes the actual grid cell");
	ClearSpeech(); Tick(); Check(speech.empty(), "stationary skill does not repeat");
	focus.widget = 72; progression.readout.focusKey = "72"; progression.readout.option = "Fila 2, columna 1";
	Check(Tick() && speech.empty(), "moving to another skill rejects the previous tooltip");
	Tooltip("Registra un cadáver.\nBuscar poción"); Tick();
	Check(speech.size() == 1 && speech[0].starts_with("Buscar poción."), "next native D-pad selection reads its skill without repeating the branch");
	ClearSpeech(); progression = {}; widgetType = {"TabWidget", UiText::Kind::TabButton}; focus.widget = 73;
	buttonSnapshot = {73, 73, "Tab1", UiText::Surface::Other, {{73, -1, UiText::Kind::TabButton, "Tab1", "", "Dominio del combate"}}};
	Check(InteractionReader::HandlesFocus(focus) && Tick() && speech == std::vector<std::string>{"Dominio del combate"}, "focused branch tabs are readable through the shared interaction pipeline");
	Passed();
}
