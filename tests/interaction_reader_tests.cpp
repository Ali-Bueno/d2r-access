#include "character_stats.h"
#include "entity_radar_service.h"
#include "interaction_reader.h"
#include "inventory_cursor.h"
#include "native_tooltips.h"
#include "native_controller_help.h"
#include "native_progression.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "test_support.h"
#include "tab_reader.h"
#include "tooltip_text.h"
#include "input_direction.h"

#include <string>
#include <vector>

namespace D2RAccess::GroundItemService {
bool ownsLabel = false;
bool OwnsWorldLabel(std::string_view label) { return ownsLabel && label.starts_with("Poción"); }
}

namespace D2RAccess::EntityRadarService {
std::string hiddenLabel;
TargetSight::Sight LabelSight(std::string_view label) {
	return !hiddenLabel.empty() && label == hiddenLabel ? TargetSight::Sight::Hidden : TargetSight::Sight::Unknown;
}
}

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
// -1 is the real function's "could not read", so the gold row stays silent.
int playerGold = -1;
std::string controllerHelp;
NativeUi::ItemPresence equipmentPresence = NativeUi::ItemPresence::Unknown;
NativeUi::ItemPresence itemPresence = NativeUi::ItemPresence::Unknown;
std::optional<bool> holdingItem;
InputDirection::StickPush pendingPush = InputDirection::StickPush::None;
// The character panel's stick reader runs ahead of the detail readout, so
// this stands in for it claiming a tick.
bool characterStatsClaims = false;

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
ItemPresence EquipmentItemPresence(uintptr_t) { return equipmentPresence; }
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
std::string ReadName(uintptr_t id) {
	if (id == 10) return "BankExpansionLayout";
	if (id == 20) return "PlayerInventoryExpansionLayout";
	if (id == 26 || id == 29) return "gold_button";
	return "widget" + std::to_string(id);
}
}
namespace D2RAccess::NativeTooltips {
void Track(uintptr_t panel) { tracked = panel; }
void Reset() { tracked = 0; tooltip = {}; }
Snapshot Latest(uint64_t) { return tracked ? tooltip : Snapshot{}; }
}
namespace D2RAccess::NativeProgression {
Details Read(NativeUi::Focus, const NativeUi::Type&) { return progression; }
}
namespace D2RAccess::NativeGroundItems {
int PlayerGold() noexcept { return playerGold; }
}
namespace D2RAccess::InventoryCursor {
std::optional<bool> HoldingItem(const D2RL::PluginContext*) { return holdingItem; }
}
namespace D2RAccess::NativeControllerHelp {
std::string ForFocus(NativeUi::Focus) { return controllerHelp; }
void Reset() {}
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
namespace D2RAccess::InputDirection {
bool TakeRightStickPushIf(StickPush wanted, const char*) {
	if (wanted == StickPush::None || pendingPush != wanted) return false;
	pendingPush = StickPush::None;
	return true;
}
}
// The damage, defense and price labels come from the game's own string table,
// as printf formats -- the reader keeps only the part before the placeholder.
// Every other key here is the localized inventory title the reader caches.
namespace D2RAccess::Localization {
std::string Text(std::string_view key) {
	if (key == "ItemStats1l") return "Daño: %d a %d";
	if (key == "ItemStats1h") return "Defensa: %d";
	if (key == TooltipText::PriceKey) return "Coste: %d";
	if (key == TooltipText::SellKey) return "Valor de venta: %d";
	for (const char* stat : TooltipText::BaseStatKeys) if (key == stat) return {};
	++titleLookups;
	return {};
}
}
namespace D2RAccess::CharacterStats {
bool HandlesFocus(NativeUi::Focus, const NativeUi::Type&) { return false; }
UiText::Readout Read(NativeUi::Focus, const NativeUi::Type&) { return {}; }
void Reset() {}
}
namespace D2RAccess::CharacterStatsReader {
bool Tick(const D2RL::PluginContext*, NativeUi::Focus) {
	if (!characterStatsClaims) return false;
	pendingPush = InputDirection::StickPush::None;
	return true;
}
void Reset() {}
}
bool autoDetail = false;
namespace D2RAccess::AutoDetail { bool Enabled() { return autoDetail; } }
namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool) { speech.emplace_back(text); }
void Stop() {}
}

int main() {
	using namespace D2RAccess;
	Strings::Select(Strings::Language::esES);
	Tooltip("Tu alijo personal");
	Check(!Settle() && speech == std::vector<std::string>{"Tu alijo personal"}, "highlighted stash name reads without controller UI focus");
	ClearSpeech(); Settle();
	Check(speech.empty(), "stationary world target does not repeat");
	tooltip = {}; Settle(); Tooltip("Tu alijo personal"); Settle();
	Check(speech == std::vector<std::string>{"Tu alijo personal"}, "returning to the same highlighted object reads again");
	ClearSpeech(); Tooltip("\xEE\x80\x93"); Settle();
	Check(speech.empty(), "a button image is not a world object name");
	GroundItemService::ownsLabel = true; Tooltip("Poción"); Settle();
	Check(speech.empty(), "proximity-owned ground item does not also announce through its tooltip");
	GroundItemService::ownsLabel = false; Settle();
	Check(speech == std::vector<std::string>{"Poción"}, "tooltip speech remains available when proximity speech is disabled");
	ClearSpeech();
	Tooltip("Arqueta"); Settle();
	Check(speech == std::vector<std::string>{"Arqueta"}, "different object labels use the same reader");
	ClearSpeech(); conversation = true; Settle();
	Check(speech.empty() && tracked == 0 && InteractionReader::Repeat().empty(), "NPC conversations suspend tooltip capture and clear old world labels");
	conversation = false; Tooltip("Akara"); Settle();
	Check(speech == std::vector<std::string>{"Akara"}, "NPC name can use the native target tooltip");
	ClearSpeech();
	EntityRadarService::hiddenLabel = "Zombi"; Tooltip("Zombi"); Settle();
	Check(speech.empty(), "a world target the walls hide is not spoken");
	EntityRadarService::hiddenLabel.clear(); Settle();
	Check(speech == std::vector<std::string>{"Zombi"}, "the same target speaks once it comes into sight");
	ClearSpeech(); tooltip = {};
	target = {40, 0, "HUDMonsterHealth", UiText::Surface::Target, {
		{41, -1, UiText::Kind::Text, "Name", "TextBoxWidget", "Charsi"},
	}};
	EntityRadarService::hiddenLabel = "Charsi"; Settle();
	Check(speech.empty(), "the monster health plate obeys the same gate");
	EntityRadarService::hiddenLabel.clear();
	Settle();
	Check(speech == std::vector<std::string>{"Charsi"}, "HUD target-name text covers targets without a tooltip");
	ClearSpeech(); focus = {50, 51}; Settle();
	Check(speech.empty(), "another focused menu suppresses background target labels");
	target = {}; focus = {10, 11}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid};
	cell = {0, 0}; selection = 0;
	inventory = {10, 11, "BankExpansionLayout", UiText::Surface::Other, {{12, -1, UiText::Kind::Tabs, "BankTabs", "TabBarWidget", "Personal"}}};
	// Item bodies are written the way the engine stores them: bottom-up, with
	// the name last. See tooltip_text.h.
	Tooltip("Recupera 45 de vida\nPoción de curación menor"); tooltip.footer = "Usar";
	Check(Settle() && speech == std::vector<std::string>{"Alijo. Personal. Poción de curación menor"}, "stash opens with its selected tab and the item name, without its description or coordinates");
	ClearSpeech(); Settle();
	Check(speech.empty(), "stationary inventory selection stays quiet");
	Check(inventoryWalks == 0 && titleLookups == 1, "localized title is cached; shared tab tracking owns discovery");
	cell = {1, 0}; selection = 1; Tick();
	Tooltip("Recupera 45 de vida\nPoción de curación menor"); tooltip.footer = "Usar";
	Settle();
	Check(speech == std::vector<std::string>{"Poción de curación menor"}, "identical items in different cells are announced without coordinates or section repetition");
	ClearSpeech(); inventory.nodes[0].text = "Comp."; Settle();
	Check(speech.size() == 1 && speech[0].starts_with("Alijo. Comp."), "changing the actual stash tab announces the new section");
	Check(inventoryWalks == 0 && titleLookups == 1, "cell navigation and selected-tab changes avoid full inventory walks");
	ClearSpeech(); cell = {2, 0}; selection = 2; Settle();
	Check(speech == std::vector<std::string>{"Fila 1, columna 3"}, "old tooltip is discarded when selection moves to a cell without a rendered description");
	ClearSpeech(); focus = {20, 21}; inventory = {}; cell = {0, 1}; selection = uint64_t{1} << 32;
	Tooltip("Pergamino del portal de la ciudad"); Settle();
	Check(speech == std::vector<std::string>{"Inventario. Pergamino del portal de la ciudad"}, "player inventory reads the item without coordinates");
	ClearSpeech(); focus.widget = 22; widgetType = {"InventorySlotWidget", UiText::Kind::InventorySlot}; cell = {};
	Tooltip("Durabilidad: 28 de 28\nDaño: 3–6\nHacha de mano"); Settle();
	Check(speech == std::vector<std::string>{"Inventario. Equipo. Hacha de mano"}, "equipment announces its subsection and the item name, and stops there");
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
	// The window announcer has just said "Personaje" for the panel it opened; the
	// menu switcher then reports that same panel as a tab change.
	PanelState::Announced("Personaje");
	Check(InteractionReader::Tick(&context, now + 150, focus, false, "Personaje") && speech.empty(),
		"a tab naming the panel the window has just announced is not said a second time");
	Check(InteractionReader::Tick(&context, now + 160, focus, false, "Personaje")
		&& speech == std::vector<std::string>{"Personaje"}, "only once: a later change to that tab still reads");
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
	Check(speech == std::vector<std::string>{Strings::Current().equipment},
		"an unknown equipment location says only that the focus is on an equipment slot, never a guessed name");
	equipmentLocation = 0; tooltip = {}; focus.widget = 26; widgetType = {"ButtonWidget", UiText::Kind::Button};
	buttonSnapshot = {26, 26, "gold_button", UiText::Surface::Other, {{26, -1, UiText::Kind::Button, "gold_button", "ButtonWidget", "Soltar oro"}}};
	Check(InteractionReader::HandlesFocus(focus), "icon buttons use native focus instead of rectangle matching");
	ClearSpeech(); Check(Tick() && speech == std::vector<std::string>{"Soltar oro"}, "gold action is announced immediately upon leaving equipment");
	ClearSpeech(); Tooltip("Soltar oro"); Settle();
	Check(speech.empty(), "matching rendered tooltip does not repeat the button label");
	// The gold row's widget carries no text, so the amount comes from the player's
	// stats. -1 is "could not read", and then the button's own label still stands.
	ClearSpeech(); playerGold = 523; focus.widget = 29;
	buttonSnapshot = {29, 29, "gold_button", UiText::Surface::Other, {{29, -1, UiText::Kind::Button, "gold_button", "ButtonWidget", "Soltar oro"}}};
	Check(Tick() && speech == std::vector<std::string>{"523 de oro"},
		"the gold row speaks how much is carried instead of the action that would drop it");
	ClearSpeech(); Tooltip("Soltar oro"); Settle();
	Check(speech.empty(), "the gold row's tooltip, which is its A hint again, does not re-read the row");
	playerGold = -1; ClearSpeech();
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
	Tooltip("Daño: 6-13\nHacha grande"); tooltip.footer = "Coste: 331";
	Check(Tick() && speech == std::vector<std::string>{"Inventario. Hacha grande"}, "delayed item reads its name immediately, and holds the rest back");
	// The rest of the description is held for the player to ask for.
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Daño: 6-13. Coste: 331"}, "right stick up reads the description the move left out, base damage first");
	Check(pendingPush == InputDirection::StickPush::None, "the push is claimed, not left for the entity scan");
	// An options row offers its explanation to the same gesture instead of
	// speaking it on arrival; a row with nothing to explain offers nothing.
	ClearSpeech(); InteractionReader::OfferDetail("GameplayOptionsPanel/ShowClock", "Shows a clock on the HUD.");
	pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Shows a clock on the HUD."},
		"an options row explains itself when asked, not on arrival");
	ClearSpeech(); InteractionReader::OfferDetail("GameplayOptionsPanel/AutoGold", {});
	pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{Strings::Current().detailNone},
		"and moving to a row with no explanation drops the previous row's");

	// A vendor writes its price where an inventory tooltip has the item name,
	// so the first line was being read as the item: MEASURED 2026-09-20,
	// "Cost: 30" for a Minor Healing Potion. Name first, then the price.
	ClearSpeech(); tooltip = {}; selection = 7; cell = {2, 0};
	Tooltip("Puntos: 45\nPoción de curación menor\nCoste: 30");
	Check(Tick() && speech.size() == 1 && speech.back() == "Poción de curación menor. Coste: 30",
		"a shop item reads its name and then what it costs");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Puntos: 45"},
		"and the rest of it waits to be asked for");
	// Selling says "Sell value", which was not a known price label, so it was
	// read as the name and the name went to the detail (MEASURED 2026-09-21).
	ClearSpeech(); tooltip = {}; selection = 8; cell = {3, 0};
	Tooltip("Durabilidad: 5 de 6\nDaño: 1 a 3\nOrbe de águila de baja calidad\nValor de venta: 73");
	Check(Tick() && speech.size() == 1 && speech.back() == "Orbe de águila de baja calidad. Valor de venta: 73",
		"selling reads the name and what it fetches, not the damage");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Daño: 1 a 3. Durabilidad: 5 de 6"},
		"and the damage is read with the rest on the right stick");
	// Ctrl+D: the whole description with the name, and the gesture still works.
	autoDetail = true;
	ClearSpeech(); tooltip = {}; selection = 9; cell = {4, 0};
	Tooltip("Durabilidad: 5 de 6\nDaño: 1 a 3\nBastón corto\nValor de venta: 40");
	Check(Tick() && speech.size() == 1 && speech.back() == "Bastón corto. Valor de venta: 40. Daño: 1 a 3. Durabilidad: 5 de 6",
		"with autodetail on, a highlight reads the whole description");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Daño: 1 a 3. Durabilidad: 5 de 6"}, "and right stick up still reads the detail");
	autoDetail = false;

	ClearSpeech(); pendingPush = InputDirection::StickPush::Down; Tick();
	Check(pendingPush == InputDirection::StickPush::Down, "any other direction still belongs to the pathfinder");
	pendingPush = InputDirection::StickPush::None;
	// The character panel claims all four directions, so it must be offered
	// the push before the detail readout takes up out from under it.
	ClearSpeech(); characterStatsClaims = true; pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech.empty() && pendingPush == InputDirection::StickPush::None,
		"the character panel's reader is offered the stick before the item detail readout");
	characterStatsClaims = false;
	ClearSpeech(); selection = 1; cell = {1, 0}; Settle();
	Check(speech.empty(), "moving to another occupied cell waits instead of reading stale item text or coordinates");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{Strings::Current().detailNone},
		"moving off an item drops its description, so the gesture can never answer about the last one");
	ClearSpeech();
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
	progression.help = "X: Asignar habilidad";
	progression.readout = {"Skills", "Gritos de guerra", "", "71"};
	progression.readout.coordinates = "Fila 1, columna 1";
	Check(Tick() && speech.empty(), "skill awaits its matching rendered description without speaking a blank button or coordinates");
	Tooltip("Huyen aterrorizados.\n\xEE\x81\xBE" "2Aullido\nSin aprender"); Tick();
	Check(speech == std::vector<std::string>{"Gritos de guerra. Aullido. 1 punto. X: Asignar habilidad. Fila 1, columna 1"}, "a skill reads its name, its points and its binding, and the grid position last");
	ClearSpeech(); Tick(); Check(speech.empty(), "stationary skill does not repeat");
	pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Sin aprender Huyen aterrorizados."}, "right stick up reads what the skill does");
	ClearSpeech();
	focus.widget = 72; progression.readout.focusKey = "72"; progression.readout.coordinates = "Fila 2, columna 1";
	Check(Tick() && speech.empty(), "moving to another skill rejects the previous tooltip");
	pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{Strings::Current().detailNone}, "and drops what the previous one did");
	ClearSpeech();
	Tooltip("Registra un cadáver.\nBuscar poción"); Tick();
	Check(speech.size() == 1 && speech[0].starts_with("Buscar poción."), "next native D-pad selection reads its skill without repeating the branch");
	ClearSpeech(); progression = {}; widgetType = {"TabWidget", UiText::Kind::TabButton}; focus.widget = 73;
	buttonSnapshot = {73, 73, "Tab1", UiText::Surface::Other, {{73, -1, UiText::Kind::TabButton, "Tab1", "", "Dominio del combate"}}};
	Check(InteractionReader::HandlesFocus(focus) && Tick() && speech == std::vector<std::string>{"Dominio del combate"}, "focused branch tabs are readable through the shared interaction pipeline");
	InteractionReader::Reset(); ClearSpeech(); progression = {}; tooltip = {}; controllerHelp.clear();
	focus = {20, 81}; widgetType = {"InventorySlotWidget", UiText::Kind::InventorySlot};
	cell = {}; equipmentLocation = 1; equipmentPresence = NativeUi::ItemPresence::Empty;
	Check(Tick() && speech == std::vector<std::string>{"Inventario. Equipo. Cabeza. Vacío"}, "empty equipment announces its location and presence on the first poll");
	ClearSpeech(); focus.widget = 82; equipmentLocation = 2; equipmentPresence = NativeUi::ItemPresence::Occupied;
	Check(Tick() && speech == std::vector<std::string>{"Cuello. Ocupado"}, "rapid slot navigation never waits for a rendered item description");
	ClearSpeech(); Tooltip("Resistencia al fuego: 10%\nAmuleto"); Tick();
	Check(speech == std::vector<std::string>{"Cuello. Amuleto"}, "delayed equipped item retains the slot name with the item's name");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Resistencia al fuego: 10%"}, "an equipped item's properties are there for the asking");
	ClearSpeech(); equipmentPresence = NativeUi::ItemPresence::Empty; Tick();
	Check(speech == std::vector<std::string>{"Cuello. Vacío"}, "removing equipment rejects a lingering tooltip even without changing focus");
	ClearSpeech(); equipmentPresence = NativeUi::ItemPresence::Occupied; tooltip = {}; Tick();
	Check(speech == std::vector<std::string>{"Cuello. Ocupado"}, "equipping into the focused empty slot announces the real occupancy change");
	ClearSpeech(); Tick(); Check(speech.empty(), "stationary occupied slot without a description does not repeat");
	InteractionReader::Reset(); ClearSpeech(); equipmentPresence = NativeUi::ItemPresence::Unknown;
	focus = {20, 21}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid}; selection = 0; cell = {0, 0};
	itemPresence = NativeUi::ItemPresence::Occupied;
	controllerHelp = "Y: Reparar todo. B: Cerrar menú";
	Check(Tick() && speech.empty(), "available actions wait for the selected item's description instead of being read ahead of it");
	ClearSpeech(); Tick(); Check(speech.empty(), "unchanged merchant actions do not repeat while waiting");
	Tooltip("Durabilidad: 15 de 28\nHacha"); Tick();
	Check(speech == std::vector<std::string>{"Inventario. Hacha. Y: Reparar todo. B: Cerrar menú"},
		"the action list is spoken after the item it applies to, in the same utterance");
	Check(InteractionReader::Repeat().ends_with(controllerHelp), "manual repeat includes currently available controller actions");
	ClearSpeech(); selection = 1; cell = {1, 0}; Tooltip("Espada"); Tick();
	Check(speech == std::vector<std::string>{"Espada"}, "item-to-item navigation does not repeat unchanged merchant actions");
	ClearSpeech(); controllerHelp = "X: Actualizar. B: Cerrar menú"; Tick();
	Check(speech == std::vector<std::string>{controllerHelp}, "a new NPC action is read without repeating the selected item");
	ClearSpeech(); controllerHelp = "B: Cerrar menú"; Tick();
	Check(speech == std::vector<std::string>{controllerHelp} && InteractionReader::Repeat().find("Reparar") == std::string::npos, "unavailable services disappear from speech and repeat");
	ClearSpeech(); conversation = true; Tick();
	Check(speech.empty() && InteractionReader::Repeat().empty(), "NPC dialogue suppresses inventory actions underneath it");
	conversation = false;
	InteractionReader::Reset(); ClearSpeech(); controllerHelp.clear(); tooltip = {};
	focus = {20, 21}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid};
	holdingItem = false; itemPresence = NativeUi::ItemPresence::Occupied;
	Tick(); Check(speech.empty(), "entering inventory without a held item does not announce a release");
	holdingItem = true; Tick();
	Check(speech == std::vector<std::string>{Strings::Current().itemInHand}, "picking up an item announces placement mode without waiting for a tooltip");
	ClearSpeech(); Tick(); Check(speech.empty(), "holding an item does not repeat the placement notice");
	focus.widget = 81; widgetType = {"InventorySlotWidget", UiText::Kind::InventorySlot}; equipmentLocation = 1;
	cell = {};
	equipmentPresence = NativeUi::ItemPresence::Empty; Tick();
	Check(speech == std::vector<std::string>{"Inventario. Equipo. Cabeza. Vacío"}, "crossing into empty equipment reads the slot without repeating placement help");
	ClearSpeech(); focus.widget = 83; cell = {2, 1}; Tick();
	Check(speech == std::vector<std::string>{"Cabeza. Vacío. Fila 2, columna 3"}, "an equipment slot speaks its grid position after its name and contents");
	ClearSpeech(); focus.widget = 84; cell = {}; Tick(); ClearSpeech();
	equipmentLocation = 0; focus.widget = 83; Tick();
	Check(speech == std::vector<std::string>{"Cabeza. Vacío"}, "a slot whose location the game briefly stops reporting keeps the name it already had");
	ClearSpeech(); focus.widget = 90; equipmentPresence = NativeUi::ItemPresence::Unknown; Tick();
	Check(speech == std::vector<std::string>{Strings::Current().equipment},
		"a slot with neither a known location nor readable contents still says what the focus is on");
	equipmentPresence = NativeUi::ItemPresence::Empty; equipmentLocation = 1;
	ClearSpeech(); focus.widget = 81; cell = {}; Tick();
	Check(InteractionReader::Repeat().starts_with(Strings::Current().itemInHand), "manual repeat retains the current item-in-hand context");
	ClearSpeech(); holdingItem.reset(); Tick();
	Check(speech.empty(), "a failed cursor query cannot announce that the item was placed");
	holdingItem = true; Tick(); Check(speech.empty(), "cursor query recovery does not repeat an unchanged mode");
	holdingItem = false; Tick();
	Check(speech.size() == 1 && speech[0].find("Cabeza") != std::string::npos
		&& speech[0] != Strings::Current().itemReleased, "releasing a carried item names the equipment slot it went into");
	Check(InteractionReader::Repeat().find(Strings::Current().itemInHand) == std::string::npos, "repeat removes stale item-in-hand context");
	ClearSpeech(); Tick(); Check(speech.empty(), "an empty cursor does not repeatedly announce release");
	InteractionReader::Reset(); holdingItem = true; Tick();
	Check(speech.size() == 1 && speech[0].starts_with(Strings::Current().itemInHand), "reopening inventory while holding an item restores orientation");
	ClearSpeech(); conversation = true; Tick();
	Check(speech.empty() && InteractionReader::Repeat().empty(), "conversation focus suppresses placement notices and resets orientation");
	conversation = false; InteractionReader::Reset(); ClearSpeech();
	focus = {20, 21}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid};
	cell = {0, 0}; selection = 0; holdingItem = false; itemPresence = NativeUi::ItemPresence::Empty;
	Settle(); ClearSpeech(); holdingItem = true; cell = {1, 0}; selection = 1; Tick();
	Check(speech.size() == 1 && speech[0].starts_with(Strings::Current().itemInHand) && speech[0].ends_with("Fila 1, columna 2"), "placement mode and its newly selected empty cell are spoken together");
	ClearSpeech(); Tick(); Check(speech.empty(), "a delayed coordinate read cannot immediately interrupt placement orientation");
	Passed();
}
