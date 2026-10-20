#include "belt_reader.h"
#include "character_stats.h"
#include "entity_cue_service.h"
#include "entity_radar_service.h"
#include "generated/item_sizes.h"
#include "ground_item_text.h"
#include "interaction_reader.h"
#include "mercenary_stats.h"
#include "inventory_cursor.h"
#include "native_entities.h"
#include "native_tooltips.h"
#include "native_controller_help.h"
#include "native_ground_items.h"
#include "native_progression.h"
#include "native_waypoints.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "test_support.h"
#include "tab_reader.h"
#include "tooltip_text.h"
#include "input_direction.h"
#include "native_belt.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace D2RAccess::GroundItemService {
bool ownsLabel = false;
bool OwnsWorldLabel(std::string_view label) { return ownsLabel && label.starts_with("Poción"); }
void NoteWorldTarget(std::string_view) {}
}

namespace D2RAccess::EntityRadarService {
std::string NearestNpc() { return "Charsi"; }
std::string hiddenLabel;
std::optional<TargetSight::Bearer> bearer; // what every other label names, when set
TargetSight::Bearer LabelTarget(std::string_view label) {
	if (!hiddenLabel.empty() && label == hiddenLabel) return {TargetSight::Sight::Hidden};
	return bearer.value_or(TargetSight::Bearer{});
}
}
namespace D2RAccess::TargetSight {
// The scan's name in place of the label's, as target_sight.cpp has it (tested there).
std::string Spoken(std::string_view label, const Bearer& bearer) {
	std::string spoken(label);
	const size_t at = bearer.name.empty() ? std::string::npos : spoken.find(bearer.worldName);
	return at == std::string::npos ? spoken : spoken.replace(at, bearer.worldName.size(), bearer.name);
}
}
namespace D2RAccess::EntityCueService {
std::optional<double> reach;
std::optional<double> NamingReach() { return reach; }
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
int playerGold = -1, stashGold = -1;
NativeUi::BeltSlot beltSlot;
std::array<uintptr_t, NativeUi::BeltSlots> beltItems{};
int unreadableBeltSlot = -1;
uintptr_t automaticPotion = 0;
std::string stashTabLabel; // the selected label of the stash's tab bar, widget 900
std::string stashGoldText; // the stash's gold_amount box, at widget 60
std::string controllerHelp;
NativeUi::ItemPresence equipmentPresence = NativeUi::ItemPresence::Unknown;
NativeUi::ItemPresence itemPresence = NativeUi::ItemPresence::Unknown;
uintptr_t gridItem = 0, equipmentItem = 0;
NativeUi::GridSlot stashItem;
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
uintptr_t GridItem(uintptr_t) { return gridItem; }
uintptr_t EquipmentItem(uintptr_t) { return equipmentItem; }
GridSlot ItemWidgetSlot(uintptr_t) { return stashItem; }
ItemPresence EquipmentItemPresence(uintptr_t) { return equipmentPresence; }
StashSlotCount StashSlotStackCount(uintptr_t) { return {}; }
int EquipmentLocation(uintptr_t) { return equipmentLocation; }
uint64_t FocusSelection(uintptr_t) { return selection; }
BeltSlot BeltItem(uintptr_t) { return beltSlot; }
BeltSlot BeltItemAt(uintptr_t, int slot) {
	if (slot == unreadableBeltSlot) return {};
	return slot >= 0 && slot < static_cast<int>(beltItems.size()) ? BeltSlot{true, slot, beltItems[slot]} : BeltSlot{};
}
UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t, uintptr_t) {
	if (panel == 40) return target;
	if (panel == focus.widget && (widgetType.kind == UiText::Kind::Button || widgetType.kind == UiText::Kind::StatButton || widgetType.kind == UiText::Kind::TabButton)) return buttonSnapshot;
	++inventoryWalks;
	return inventory;
}
std::vector<uintptr_t> FindControls(uintptr_t, UiText::Kind) { return stashTabLabel.empty() ? std::vector<uintptr_t>{} : std::vector<uintptr_t>{900}; }
// The skill tree's branch poll finds no tree in these fixtures.
bool Visible(uintptr_t widget) { return widget != 0; }
std::vector<uintptr_t> Children(uintptr_t) { return {}; }
std::string TabLabel(uintptr_t) { return {}; }
std::string SelectedTabText(uintptr_t widget) {
	if (widget == 900) return stashTabLabel;
	for (const auto& node : inventory.nodes) if (node.id == widget) return node.text;
	return {};
}
std::string FocusedTabText(uintptr_t) {
	constexpr const char* labels[] = {"Grupo", "Reg. de búsquedas", "Personaje", "Inventario", "Árbol de hab.", "Mercenario", "Opciones"};
	const auto x = static_cast<int32_t>(selection);
	return x >= 0 && x < 7 ? labels[x] : "";
}
}
namespace D2RAccess::NativeMemory { bool Copy(uintptr_t, void*, size_t) noexcept { return false; } }
namespace D2RAccess::NativeWidgets {
std::vector<FoundString> ScanStrings(uintptr_t, size_t) { return {}; }
bool IsAvailable() { return true; }
uintptr_t FindPanel(std::string_view name) { return name == "TooltipsPanel" ? 30 : name == "HUDMonsterHealth" ? 40 : 0; }
std::string ReadName(uintptr_t id) {
	if (id == 201) return "r07";
	if (id == 202) return "gcw";
	if (id == 10) return "BankExpansionLayout";
	if (id == 20) return "PlayerInventoryExpansionLayout";
	if (id == 26 || id == 29) return "gold_button";
	if (id >= 31 && id <= 33) return "gold_withdraw";
	return "widget" + std::to_string(id);
}
uintptr_t FindChild(uintptr_t parent, std::string_view name) { return parent == 10 && name == "gold_amount" ? 60 : 0; }
std::string ReadText(uintptr_t id) { return id == 60 ? stashGoldText : std::string{}; }
}
namespace D2RAccess::NativeTooltips {
void Track(uintptr_t panel) { tracked = panel; }
void Reset() { tracked = 0; tooltip = {}; }
Snapshot Latest(uint64_t) { return tracked ? tooltip : Snapshot{}; }
}
namespace D2RAccess::HireStats {
bool Tick(const D2RL::PluginContext*, uint64_t, NativeUi::Focus) { return false; }
void Reset() {}
}
namespace D2RAccess::AdvancedStatsReader {
bool Tick(const D2RL::PluginContext*, NativeUi::Focus) { return false; }
}
namespace D2RAccess::PartyReader {
bool Tick(const D2RL::PluginContext*, NativeUi::Focus) { return false; }
}
namespace D2RAccess::TooltipDelay {
void Apply(const D2RL::PluginContext*, uintptr_t) {}
}
namespace D2RAccess::NativeProgression {
Details Read(NativeUi::Focus, const NativeUi::Type&) { return progression; }
}
namespace D2RAccess::NativeBelt {
std::optional<uintptr_t> AutomaticItem(const D2RL::PluginContext*, uintptr_t, int) { return automaticPotion; }
}
// No mercenary here.
namespace D2RAccess::MercenaryStats {
std::string Name(uintptr_t) { return {}; }
std::string LevelAndType(uintptr_t) { return {}; }
std::string Experience(uintptr_t) { return {}; }
std::vector<std::string> Lines(uintptr_t) { return {}; }
std::vector<Skill> Skills(uintptr_t) { return {}; }
std::vector<std::string> SkillLines(uintptr_t) { return {}; }
}
// No skill named here: every skill tooltip is taken, as before the check.
namespace D2RAccess::NativeSkillIcons { std::string Name(uintptr_t) { return {}; } }
namespace D2RAccess::NativeGroundItems {
int PlayerGold() noexcept { return playerGold; }
int PlayerStashGold() noexcept { return stashGold; }
std::string ItemBaseName(uintptr_t unit) { return unit == 77 ? "Poción de curación menor" : std::string{}; }
// Distinct mod-owned test units cover magic/superior plus set/unique headlines.
UnitLabel ItemUnitLabel(uintptr_t unit) {
	return unit == 80 ? UnitLabel{"Lanza", 4} : unit == 81 ? UnitLabel{"Espada corta", 3}
		: unit == 82 ? UnitLabel{"Sabre", 5} : unit == 83 ? UnitLabel{"Mace", 7} : UnitLabel{};
}
UnitLabel ClassLabel(uint32_t, uint32_t quality) { return {"", quality}; }
std::optional<uint32_t> ClassOfCode(uint32_t) { return std::nullopt; }
bool SocketFiller(uint32_t) { return false; }
uint32_t ItemTableCode(uintptr_t unit) {
	return unit == 55 ? Generated::ItemCode("ssd") : unit == 77 ? Generated::ItemCode("hp1") : 0;
}
}
namespace D2RAccess::NativeEntities { std::optional<uint32_t> FilledSockets(uintptr_t) { return std::nullopt; } }
namespace D2RAccess::NativeWaypoints {
Row Read(uintptr_t, std::string_view) noexcept { return {}; }
const char* Name(State) { return ""; }
}
namespace D2RAccess::InventoryCensus {
void Log(const D2RL::PluginContext*, NativeUi::Focus, uint64_t) {}
}
namespace D2RAccess::PadFilter {
int holds = 0;
void HoldB(uint64_t) { ++holds; }
void ReleaseB() { holds = 0; }
int leftStickArms = 0;
void ArmLeftStickWithY(uint64_t) { ++leftStickArms; }
}
namespace D2RAccess::InventoryCursor {
std::optional<bool> HoldingItem(const D2RL::PluginContext*) { return holdingItem; }
std::optional<ItemState> Held(const D2RL::PluginContext*) { return holdingItem ? std::optional{ItemState{holdingItem.value() ? 1u : 0u}} : std::nullopt; }
std::optional<ItemState> Find(const D2RL::PluginContext*, uint32_t) { return std::nullopt; }
std::optional<uint32_t> HeldItemId(const D2RL::PluginContext*) { return holdingItem.value_or(false) ? 1u : 0u; }
bool Holds(const D2RL::PluginContext*, D2RL::Items::ItemContainer, uint32_t) { return false; }
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
FaceButton LastFaceButton(unsigned) { return FaceButton::None; }
bool TakeRightStickPushIf(StickPush wanted, const char*) {
	if (wanted == StickPush::None || pendingPush != wanted) return false;
	pendingPush = StickPush::None;
	return true;
}
StickPush PeekRightStickPush() { return pendingPush; }
bool lineStep = false;
bool LineStepPending() { return lineStep && pendingPush != StickPush::None; }
bool LineStepPlain() { return false; }
bool pressB = false;
bool holdingY = false;
bool FaceButtonHeld(FaceButton button) { return button == FaceButton::Y && holdingY; }
StickPush pendingLeft = StickPush::None;
StickPush PeekLeftStickPush() { return pendingLeft; }
bool TakeLeftStickPushIf(StickPush wanted, const char*) {
	if (wanted == StickPush::None || pendingLeft != wanted) return false;
	pendingLeft = StickPush::None;
	return true;
}
bool TakeFacePress(FaceButton button) { const bool pressed = button == FaceButton::B && pressB; if (button == FaceButton::B) pressB = false; return pressed; }
}
// The damage, defense and price labels come from the game's own string table,
// as printf formats -- the reader keeps only the part before the placeholder.
// Every other key here is the localized inventory title the reader caches.
namespace D2RAccess::Localization {
std::string Text(std::string_view key) {
	if (key == "r07") return "Runa Tal";
	if (key == "gcw") return "Diamante fragmentado";
	if (key == "personal") return "Personal";
	if (key == "currentEquip") return "Actualmente equipado";
	if (key == "shared") return "Compartido";
	if (key == "ControllerHelpBelt1") return "Casilla de cinturón 1 ";
	if (key == "ControllerHelpBelt2") return "Casilla de cinturón 2 ";
	if (key == "ControllerHelpBelt3") return "Casilla de cinturón 3 ";
	if (key == "ControllerHelpBelt4") return "Casilla de cinturón 4 ";
	if (key == "ItemStats1l") return "Daño: %d a %d";
	if (key == "ItemStats1h") return "Defensa: %d";
	if (key == TooltipText::UnidentifiedKey) return "Sin identificar";
	if (key == TooltipText::PriceKey) return Strings::Selected() == Strings::Language::enUS ? "Cost: %d" : "Coste: %d";
	if (key == TooltipText::SellKey) return "Valor de venta: %d";
	if (key == GroundItemText::SuffixOnlyFormatKey) return "a0n1:%1 %0"; // the game's esES entry
	for (const char* stat : TooltipText::BaseStatKeys) if (key == stat) return {};
	if (key == "ItemStats1e") return "Fuerza necesaria: %d";
	for (const char* requirement : TooltipText::RequirementKeys) if (key == requirement) return {};
	for (const char* quantity : TooltipText::QuantityKeys) if (key == quantity) return {};
	++titleLookups;
	return {};
}
}
namespace D2RAccess::CharacterStats {
bool HandlesFocus(NativeUi::Focus, const NativeUi::Type&) { return false; }
bool HasPoints(std::string_view points) { return points.find_first_of("123456789") != std::string_view::npos; }
UiText::Readout Read(NativeUi::Focus, const NativeUi::Type&) { return {}; }
void Reset() {}
}
std::string statPointsFor; // what the stats reader hands the Strength button's announcement
namespace D2RAccess::CharacterStatsReader {
std::string PointsFor(uintptr_t, uint64_t) { return statPointsFor; }
bool Tick(const D2RL::PluginContext*, NativeUi::Focus) {
	if (!characterStatsClaims) return false;
	pendingPush = InputDirection::StickPush::None;
	return true;
}
void Reset() {}
}
bool autoDetail = false;
bool controlTooltips = true;
bool inPlay = false;
namespace D2RAccess::AutoDetail { bool Enabled() { return autoDetail; } bool ControlTooltips() { return controlTooltips; } }
namespace D2RAccess::RadarUi { bool InPlay() { return inPlay; } }
// The mercenary's level-up line, named by the game text reader's switch.
namespace D2RAccess::GameTextReader {
std::string NameSpeaker(std::string text) { return text == "Me siento mucho mas fuerte" ? "Mercenario dice: " + text : text; }
}
std::vector<bool> interrupts;
int stops = 0;
namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool interrupt) { speech.emplace_back(text); interrupts.push_back(interrupt); }
void Lead(std::string_view text, unsigned long long) { Speak(text, false); }
void Stop() { ++stops; }
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
	ClearSpeech(); Tooltip("Me siento mucho mas fuerte"); Settle();
	Check(speech == std::vector<std::string>{"Mercenario dice: Me siento mucho mas fuerte"},
		"the mercenary's level-up line, drawn as the world target, says who speaks");
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
	// The auto-target trading a pack's names out of reach (log, 2026-09-29).
	ClearSpeech(); EntityCueService::reach = 3.0;
	EntityRadarService::bearer = TargetSight::Bearer{TargetSight::Sight::Visible, EntityRadar::Kind::Enemy, false, 12};
	target = {40, 0, "HUDMonsterHealth", UiText::Surface::Target, {
		{41, -1, UiText::Kind::Text, "Name", "TextBoxWidget", "Trinchante. Demonio"},
	}};
	Settle();
	Check(speech.empty(), "a monster beyond attack reach is not named");
	EntityRadarService::bearer->distance = 2; Settle();
	Check(speech == std::vector<std::string>{"Trinchante. Demonio"} && !interrupts.back(),
		"within reach it is named without cutting other speech off");
	ClearSpeech();
	const std::string shaman = "Chamán trinchante. Demonio \xEE\x81\xBE" "5\xE2\x80\xA2\xEE\x81\xBE" "0Invoca trinchantes";
	target.nodes[0].text = shaman; Settle();
	Check(speech == std::vector<std::string>{"Chamán trinchante. Demonio, Invoca trinchantes"},
		"a second monster is named once, its affix dot spoken as a pause");
	ClearSpeech();
	for (int round = 0; round < 6; ++round) {
		target.nodes[0].text = round % 2 ? shaman : "Trinchante. Demonio";
		Tick();
	}
	Check(speech.empty(), "the target trading names inside a pack is not repeated");
	// Live 2026-10-02: a Fallen joined a rat on the plate for under the reader's
	// settle, the target hopped back, and the Fallen was never named at all.
	target.nodes[0].text = "Caído. Demonio"; Tick();
	target.nodes[0].text = "Trinchante. Demonio"; Tick();
	Check(speech == std::vector<std::string>{"Caído. Demonio"}, "a new name on the plate for one poll is still said");
	ClearSpeech();
	EntityRadarService::bearer = TargetSight::Bearer{TargetSight::Sight::Visible, EntityRadar::Kind::Enemy, true, 12};
	target.nodes[0].text = "Rakanishu. Demonio"; Settle();
	Check(speech == std::vector<std::string>{"Rakanishu. Demonio"}, "a boss is named from first sight");
	ClearSpeech(); target = {}; EntityRadarService::bearer.reset(); EntityCueService::reach.reset(); Settle();
	ClearSpeech(); focus = {50, 51}; Settle();
	Check(speech.empty(), "another focused menu suppresses background target labels");
	target = {}; focus = {10, 11}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid};
	cell = {0, 0}; selection = 0;
	inventory = {10, 11, "BankExpansionLayout", UiText::Surface::Other, {{12, -1, UiText::Kind::Tabs, "BankTabs", "TabBarWidget", "Personal"}}};
	// Item bodies are written the way the engine stores them: bottom-up, with
	// the name last. See tooltip_text.h.
	Tooltip("Recupera 45 de vida\nPoción de curación menor"); tooltip.footer = "Usar";
	Check(Settle() && speech == std::vector<std::string>{"Personal. Poción de curación menor"}, "stash opens with its selected tab and the item name, without its title, description or coordinates");
	ClearSpeech(); Settle();
	Check(speech.empty(), "stationary inventory selection stays quiet");
	Check(inventoryWalks == 0 && titleLookups == 0, "the stash has no title to look up; shared tab tracking owns discovery");
	cell = {1, 0}; selection = 1; Tick();
	Tooltip("Recupera 45 de vida\nPoción de curación menor"); tooltip.footer = "Usar";
	Settle();
	Check(speech == std::vector<std::string>{"Poción de curación menor"}, "identical items in different cells are announced without coordinates or section repetition");
	ClearSpeech(); inventory.nodes[0].text = "Comp."; Settle();
	Check(speech.size() == 1 && speech[0].starts_with("Comp."), "changing the actual stash tab announces the new section");
	Check(inventoryWalks == 0 && titleLookups == 0, "cell navigation and selected-tab changes avoid full inventory walks");
	ClearSpeech(); cell = {2, 0}; selection = 2; Settle();
	Check(speech.empty(), "a cell that cannot be read is not given the position that says it is empty");
	itemPresence = NativeUi::ItemPresence::Empty; Settle();
	Check(speech == std::vector<std::string>{"Fila 1, columna 3"}, "old tooltip is discarded when selection moves to a cell without a rendered description");
	ClearSpeech(); focus = {20, 21}; inventory = {}; cell = {0, 1}; selection = uint64_t{1} << 32;
	Tooltip("Pergamino del portal de la ciudad"); Settle();
	Check(speech == std::vector<std::string>{"Inventario. Pergamino del portal de la ciudad"}, "player inventory reads the item without coordinates");
	// The style command's red index ('1') marks a requirement this character does not meet.
	ClearSpeech(); cell = {1, 1}; selection = (uint64_t{1} << 32) | 1;
	Tooltip("\xEE\x81\xBE" "1Fuerza necesaria: 40\n" "\xEE\x81\xBE" "0Armadura de placas"); Settle();
	Check(speech == std::vector<std::string>{"Armadura de placas. No se puede equipar: Fuerza necesaria: 40"},
		"a requirement drawn red is said with the item's name");
	ClearSpeech(); cell = {0, 1}; selection = uint64_t{1} << 32;
	Tooltip("Pergamino del portal de la ciudad"); Settle(); ClearSpeech();
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
	ClearSpeech(); tooltip = {}; cell = {1, 0}; selection = 1; itemPresence = NativeUi::ItemPresence::Empty;
	Tick();
	Check(speech.empty(), "new cell permits a brief wait for the renderer");
	now += 33; InteractionReader::Tick(&context, now, focus, false);
	Check(speech == std::vector<std::string>{"Fila 1, columna 2"}, "coordinate navigation speaks by the next 30 Hz poll");
	ClearSpeech();
	InteractionReader::Tick(&context, now + 33, focus, false, "Gemas");
	Check(speech == std::vector<std::string>{"Gemas"}, "switching tabs announces only the changed tab name");
	ClearSpeech(); Settle();
	Check(speech.empty(), "unchanged inventory cell does not interrupt the tab announcement on the next poll");
	ClearSpeech(); interrupts.clear(); cell = {2, 0}; selection = 2; Settle();
	Check(speech == std::vector<std::string>{"Fila 1, columna 3"} && !interrupts.back(), "the first readout after a tab change queues behind its title");
	ClearSpeech(); interrupts.clear(); cell = {3, 0}; selection = 3; Settle();
	Check(speech == std::vector<std::string>{"Fila 1, columna 4"} && interrupts.back(), "and the next one interrupts as usual");
	ClearSpeech(); cell = {1, 0}; selection = 1;
	{
		// Focus leaving the inventory for another tab lets that tab's title be;
		// leaving it for play silences the item readout.
		const auto before = stops;
		const auto kept = focus; const auto keptType = widgetType;
		focus = {50, 51}; widgetType = {}; Tick();
		Check(stops == before, "moving from the inventory to another tab does not stop the speech already under way");
		focus = kept; widgetType = keptType; Settle(); ClearSpeech();
		inPlay = true; focus = {}; widgetType = {}; Tick(); inPlay = false;
		Check(stops == before + 1, "leaving the inventory for play still silences it");
		focus = kept; widgetType = keptType; Settle(); ClearSpeech();
		// A tab change that brings focus to another panel reads that panel's
		// first highlight after the title, queued, without the title again.
		const auto keptTabs = inventory; inventory = {};
		focus = {50, 51}; widgetType = {}; Settle(); ClearSpeech(); interrupts.clear();
		focus = {20, 21}; widgetType = keptType; cell = {0, 0}; selection = 0;
		itemPresence = NativeUi::ItemPresence::Occupied; Tooltip("Pergamino");
		now += 33; InteractionReader::Tick(&context, now, focus, false, "Inventario");
		Settle();
		Check(speech == std::vector<std::string>{"Inventario", "Pergamino"} && !interrupts.back(),
			"a tab title is followed by the new panel's first highlight, queued and without the title twice");
		inventory = keptTabs;
		focus = kept; widgetType = keptType; tooltip = {}; itemPresence = NativeUi::ItemPresence::Empty; Settle(); ClearSpeech();
	}
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
	// TabReader holds the switcher under a dialogue; what it still reports is a vendor's LT/RT
	// before the cursor reaches a grid (a tester, 2026-10-18).
	Check(!InteractionReader::Tick(&context, now + 200, focus, true, "Armas") && speech == std::vector<std::string>{"Armas"},
		"a tab reported under a dialogue is still said");
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
	// The stash's gold box cannot take focus, so its withdraw button says the
	// amount the box shows, and the personal stash's stat when the box is unreadable.
	const auto savedPanel = focus.panel;
	tooltip = {}; focus = {10, 31}; stashGoldText = "12.345";
	buttonSnapshot = {31, 31, "gold_withdraw", UiText::Surface::Other, {{31, -1, UiText::Kind::Button, "gold_withdraw", "ButtonWidget", "Retirar"}}};
	Check(Tick() && speech == std::vector<std::string>{"12345 de oro en el alijo"}, "the withdraw button speaks the stash's gold box");
	ClearSpeech(); stashTabLabel = "Compartido"; focus.widget = 31; Tick();
	Check(!speech.empty() && speech.back() == "12345 de oro en el alijo compartido", "on the shared tab the gold box is the shared stash's");
	ClearSpeech(); stashTabLabel = "Personal"; focus.widget = 31; Tick();
	Check(!speech.empty() && speech.back() == "12345 de oro en el alijo personal", "on the personal tab, the personal stash's");
	ClearSpeech(); stashGoldText.clear(); stashGold = 700; focus.widget = 32; stashTabLabel = "Compartido";
	buttonSnapshot = {32, 32, "gold_withdraw", UiText::Surface::Other, {{32, -1, UiText::Kind::Button, "gold_withdraw", "ButtonWidget", "Retirar"}}};
	Tick();
	Check(std::none_of(speech.begin(), speech.end(), [](const std::string& line) { return line.find("700") != std::string::npos; }),
		"the personal stash's stat is not the shared stash's gold");
	ClearSpeech(); stashTabLabel = "Personal"; focus.widget = 33;
	buttonSnapshot = {33, 33, "gold_withdraw", UiText::Surface::Other, {{33, -1, UiText::Kind::Button, "gold_withdraw", "ButtonWidget", "Retirar"}}};
	Check(Tick() && speech == std::vector<std::string>{"700 de oro en el alijo personal"}, "an unreadable gold box falls back to the stash gold stat");
	stashTabLabel.clear();
	stashGold = -1; focus.panel = savedPanel; ClearSpeech();
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
	Check(pendingPush == InputDirection::StickPush::None, "the push is claimed, not left for the level scanner");
	// Ctrl with an arrow key walks the same description a line at a time.
	ClearSpeech(); InputDirection::lineStep = true; pendingPush = InputDirection::StickPush::Down;
	Check(Tick() && speech == std::vector<std::string>{"Daño: 6-13"}, "a line step down reads the line after the name");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Down; Tick();
	pendingPush = InputDirection::StickPush::Down; Tick();
	Check(speech == std::vector<std::string>{"Coste: 331", "Coste: 331"}, "the footer is the last line, and past it the last is read again");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Left; Tick();
	pendingPush = InputDirection::StickPush::Right; Tick();
	pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech == std::vector<std::string>{"Hacha grande", "Coste: 331", "Daño: 6-13"}, "left is the first line, right the last, up the one before");
	InputDirection::lineStep = false;
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

	// In play, a push down belongs to the level scanner; in a menu it asks for
	// the controls.
	ClearSpeech(); inPlay = true; pendingPush = InputDirection::StickPush::Down; Tick();
	Check(pendingPush == InputDirection::StickPush::Down, "in play a push down is left for the level scanner");
	inPlay = false; ClearSpeech(); Tick();
	Check(pendingPush == InputDirection::StickPush::None && speech.size() == 1, "in a menu it is claimed, for the controls");
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
	// Magic and better say their quality after the name; a lesser item's own
	// name already says it; an unidentified item says that instead.
	ClearSpeech(); gridItem = 80; Tooltip("Daño: 6-13\nLanza carmesí"); Tick();
	Check(speech == std::vector<std::string>{"Lanza carmesí. mágico"}, "a magic item says its quality after its name");
	ClearSpeech(); Tooltip("Sin identificar\nLanza"); Tick();
	Check(speech == std::vector<std::string>{"Lanza. Sin identificar"}, "an unidentified item says so in place of its quality");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech.size() == 1 && speech.back().find("Sin identificar") == std::string::npos, "and it is not repeated in the detail");
	ClearSpeech(); gridItem = 81; Tooltip("Espada corta superior"); Tick();
	Check(speech == std::vector<std::string>{"Espada corta superior"}, "a lesser item keeps its own name, with no quality word");
	// The set composer styles the name with index 3 and the base type with 2.
	// Index 2 used to win merely for its colour, reproducing the user's Sabre
	// headline even though the very first description line named Angelic Sickle.
	ClearSpeech(); gridItem = 82;
	Tooltip("\xEE\x81\xBE" "2Angelic Raiment\n\xEE\x81\xBE" "1Fuerza necesaria: 25\n"
		"\xEE\x81\xBE" "0Daño: 3 a 8\n\xEE\x81\xBE" "2Sabre\n\xEE\x81\xBE" "3Angelic Sickle");
	Tick();
	Check(speech.size() == 1 && speech.front().starts_with(std::string("Angelic Sickle. ") + Strings::Current().itemQualities[5]),
		"a set item's displayed full name leads its quality, never the styled base type or set title");
	Check(speech.front().find("Fuerza necesaria: 25") != std::string::npos,
		"set name correction preserves the red unmet requirement on the short readout");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech.size() == 1 && speech.front().starts_with("Sabre. Daño: 3 a 8")
		&& speech.front().find("Angelic Raiment") != std::string::npos,
		"a set item's type, properties and set title stay available in the detail");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Left; InputDirection::lineStep = true; Tick();
	Check(speech == std::vector<std::string>{"Angelic Sickle"}, "line reader and short readout agree on the displayed set name");
	InputDirection::lineStep = false;
	ClearSpeech(); Tooltip("Sin identificar\n\xEE\x81\xBE" "2Sabre"); Tick();
	Check(speech == std::vector<std::string>{"Sabre. Sin identificar"}, "unidentified set items keep their visible type and unidentified status");
	ClearSpeech(); gridItem = 83; Tooltip("Daño: 3 a 8\nMace\n\xEE\x81\xBE" "2Crushflange"); Tick();
	Check(speech == std::vector<std::string>{std::string("Crushflange. ") + Strings::Current().itemQualities[7]},
		"unique full names and their quality remain unchanged beside the set-item fix");
	gridItem = 0;
	// A belt slot's item is named by its tooltip, now drawn at once; an empty
	// slot, which has no tooltip, says so from the slot. Each says its D-pad
	// button and row first and its column's count last.
	ClearSpeech(); tooltip = {}; cell = {}; focus = {20, 23}; widgetType = {"BeltWidget", UiText::Kind::InventoryItem};
	selection = 0; beltSlot = {true, 0, 77};
	Settle();
	Check(speech.empty(), "an occupied belt slot waits for its item's tooltip");
	Tooltip("Puntos: 45\nPoción de curación menor"); Settle();
	Check(!speech.empty() && speech.back().find("Poción de curación menor") != std::string::npos, "and is read with it");
	ClearSpeech(); selection = 1; beltSlot = {true, 1, 0}; tooltip = {};
	Settle();
	Check(speech == std::vector<std::string>{"Casilla de cinturón 2 Cruceta arriba. Fila 1. Vacío. 0 en esta columna"},
		"an empty belt slot names its native D-pad direction and confirms its empty column");
	ClearSpeech(); selection = 2; beltSlot = {true, 2, 77}; beltItems[2] = 77;
	Settle(); Tooltip("Puntos: 45\nPoción de curación menor"); Settle();
	Check(speech == std::vector<std::string>{"Casilla de cinturón 3 Cruceta abajo. Fila 1. Poción de curación menor. 1 en esta columna"},
		"the next slot reads its button, its item once its tooltip is drawn, and its column");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Puntos: 45"}, "while its detail still reaches the right stick");
	ClearSpeech(); selection = 3; beltSlot = {}; tooltip = {};
	Settle();
	Check(speech.empty(), "a slot that cannot be read waits for the tooltip, as before");
	ClearSpeech(); selection = 7; beltSlot = {true, 7, 77}; beltItems = {}; beltItems[3] = 77; beltItems[11] = 77;
	Settle(); Tooltip("Puntos: 45\nPoción de curación menor"); Settle();
	Check(speech.size() == 1 && speech[0].find("Cruceta derecha. Fila 2") != std::string::npos
		&& speech[0].find("3 en esta columna") != std::string::npos,
		"reserve rows retain the column's real button and count only that column");
	unreadableBeltSlot = 11;
	Check(BeltReader::Read(&context, focus.widget).summary.empty(),
		"one unreadable reserve suppresses a misleading partial column count");
	unreadableBeltSlot = -1;
	Check(BeltReader::Location(-1).empty() && BeltReader::Location(NativeUi::BeltSlots).empty(),
		"unknown selection cannot claim a controller assignment");
	ClearSpeech(); tooltip = {}; holdingItem = true; Tick(); ClearSpeech(); holdingItem = false; Tick();
	Check(!speech.empty() && speech[0].find("Colocado en Casilla de cinturón 4 Cruceta derecha. Fila 2") != std::string::npos,
		"placing a potion names its button and row");
	ClearSpeech(); selection = 0; beltSlot = {true, 0, 0}; beltItems = {}; automaticPotion = 77;
	Settle();
	Check(speech.size() == 1 && speech[0].find("Poción de curación menor") != std::string::npos
		&& speech[0].find("Poción automática del inventario") != std::string::npos
		&& speech[0].find("0 en esta columna") != std::string::npos,
		"a native automatic potion is named without counting inventory as physical belt reserves");
	automaticPotion = 0; beltItems = {}; holdingItem.reset();
	beltSlot = {};
	conversation = false; InteractionReader::Reset(); ClearSpeech();
	focus = {60, 61}; widgetType = {"CharacterStatLevelUpButtonWidget", UiText::Kind::StatButton};
	buttonSnapshot = {61, 61, "StrengthPointButton", UiText::Surface::Other, {{61, -1, UiText::Kind::StatButton, "StrengthPointButton", "", "Gastar punto"}}};
	progression.readout = {"Stats", "", "Fuerza. 30", "61"};
	progression.value = "30"; progression.points = "5 puntos"; progression.description = "Aumenta el daño.";
	controllerHelp = "A: Asignar un punto. Vista: Ver estadísticas avanzadas. B: Cerrar menú";
	statPointsFor = "5 puntos";
	Check(InteractionReader::HandlesFocus(focus) && Tick() && speech == std::vector<std::string>{"Fuerza. 30. 5 puntos"},
		"opened onto Strength with points to spend, the button's announcement ends with them");
	statPointsFor.clear(); InteractionReader::Reset(); ClearSpeech();
	Check(InteractionReader::HandlesFocus(focus) && Tick() && speech == std::vector<std::string>{"Fuerza. 30"},
		"with autodetail off a stat reads its label and value, and nothing else");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Aumenta el daño."}, "right stick up reads what the stat does");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Down;
	Check(Tick() && speech == std::vector<std::string>{controllerHelp},
		"right stick down reads the controls to spend a point, BACK's View Advanced Stats included");
	controllerHelp.clear();
	ClearSpeech(); Tooltip("Gastar punto"); Settle();
	Check(speech.empty(), "rendered stat action does not repeat generic spend instruction");
	buttonSnapshot.nodes[0].enabled = false; Tick();
	Check(speech.size() == 1 && speech[0].ends_with("no disponible"), "enriched stat readout preserves disabled state");
	ClearSpeech(); progression.value = "31"; progression.points = "4 puntos";
	progression.readout.option = "Fuerza. 31"; Tick();
	Check(speech == std::vector<std::string>{"31"}, "spending a stat point reads only the new value: right stick right has the points left");
	ClearSpeech(); progression.value = "32"; progression.points = "3 puntos"; Tick();
	Check(speech == std::vector<std::string>{"32"}, "a new value is said even when the readout's own text has not caught up");
	autoDetail = true; ClearSpeech(); focus.widget = 62; progression.readout.focusKey = "62";
	buttonSnapshot = {62, 62, "DexPointButton", UiText::Surface::Other, {{62, -1, UiText::Kind::StatButton, "DexPointButton", "", "Gastar punto"}}};
	progression.readout.option = "Destreza. 20"; progression.value = "20"; progression.description = "Aumenta la precisión."; Tick();
	Check(speech == std::vector<std::string>{"Destreza. 20. Aumenta la precisión."}, "with autodetail on a stat reads its value and what it does");
	autoDetail = false;
	InteractionReader::Reset(); ClearSpeech(); progression = {}; tooltip = {};
	focus = {70, 71}; widgetType = {"ButtonWidget", UiText::Kind::Button}; buttonSnapshot = {};
	progression.skill = true; progression.points = "1 punto";
	progression.help = "Asignada a A";
	progression.controls = "X: Asignar habilidad";
	progression.readout = {"Skills", "Gritos de guerra", "", "71"};
	progression.readout.coordinates = "Fila 1, columna 1";
	Check(Tick() && speech.empty(), "skill awaits its matching rendered description without speaking a blank button or coordinates");
	Tooltip("Huyen aterrorizados.\n\xEE\x81\xBE" "2Aullido\nSin aprender"); Tick();
	Check(speech == std::vector<std::string>{"Gritos de guerra. Aullido. Asignada a A. Fila 1, columna 1"}, "a skill reads its name and its binding, and the grid position last");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Right;
	Check(Tick() && speech == std::vector<std::string>{"1 punto"}, "the choices left are read on right stick right, as the character panel's points are");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Down;
	Check(Tick() && speech == std::vector<std::string>{"X: Asignar habilidad"}, "what the buttons do waits for right stick down");
	ClearSpeech(); Tick(); Check(speech.empty(), "stationary skill does not repeat");
	pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Sin aprender. Huyen aterrorizados."}, "right stick up reads what the skill does, a pause between its lines");
	// Right stick left opens the skill line by line; up and down move, and
	// right stick left or B closes it and reads the skill again.
	ClearSpeech(); pendingPush = InputDirection::StickPush::Left;
	Check(Tick() && speech == std::vector<std::string>{"Aullido"} && PadFilter::holds > 0, "right stick left opens the skill's lines at its name, holding B from the game");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Down; Tick();
	pendingPush = InputDirection::StickPush::Down; Tick();
	Check(speech == std::vector<std::string>{"Sin aprender", "Huyen aterrorizados."}, "down reads each following line, with no number");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Down; Tick();
	Check(speech == std::vector<std::string>{"Huyen aterrorizados."}, "past the last line the last is read again");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech == std::vector<std::string>{"Sin aprender"}, "up goes back");
	ClearSpeech(); InputDirection::pressB = true; Tick();
	Check(speech.size() == 1 && speech[0].starts_with("Gritos de guerra. Aullido") && PadFilter::holds == 0, "B closes it, lets B go, and reads the skill again");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech == std::vector<std::string>{"Sin aprender. Huyen aterrorizados."}, "closed, up is the detail again");
	ClearSpeech(); InputDirection::pressB = true; pendingPush = InputDirection::StickPush::Left; Tick();
	Check(speech == std::vector<std::string>{"Aullido"}, "reopening ignores a B pressed before it opened");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Left; Tick();
	Check(speech.size() == 1 && speech[0].starts_with("Gritos de guerra. Aullido"), "right stick left closes it too");
	ClearSpeech();
	focus.widget = 72; progression.readout.focusKey = "72"; progression.readout.coordinates = "Fila 2, columna 1";
	Check(Tick() && speech.empty(), "moving to another skill rejects the previous tooltip");
	pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{Strings::Current().detailNone}, "and drops what the previous one did");
	ClearSpeech();
	Tooltip("Registra un cadáver.\nBuscar poción"); Tick();
	Check(speech.size() == 1 && speech[0].starts_with("Buscar poción."), "next native D-pad selection reads its skill without repeating the branch");
	// J and Ctrl+arrows both consume the description for this current owner.
	progression.prerequisites = {"Might", "Holy Bolt"};
	ClearSpeech(); Tick();
	pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech.size() == 1 && speech[0].find("Requisito: 1 punto de habilidad en Might") != std::string::npos
		&& speech[0].find("Requisito: 1 punto de habilidad en Holy Bolt") != std::string::npos,
		"the J/right-stick detail command includes all prerequisites without an arrival announcement");
	ClearSpeech(); InputDirection::lineStep = true;
	pendingPush = InputDirection::StickPush::Right; Tick();
	Check(speech == std::vector<std::string>{"Requisito: 1 punto de habilidad en Holy Bolt"},
		"Ctrl+right reaches the last prerequisite in the same skill buffer");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech == std::vector<std::string>{"Requisito: 1 punto de habilidad en Might"},
		"Ctrl+up reads the preceding prerequisite as its own line");
	InputDirection::lineStep = false;
	focus.widget = 74; progression.readout.focusKey = "74"; progression.prerequisites.clear();
	tooltip = {}; ClearSpeech(); Tick(); pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech == std::vector<std::string>{Strings::Current().detailNone},
		"moving to a pending skill clears the previous skill's prerequisite detail");
	ClearSpeech(); Tooltip("Nueva descripción.\nNuevo skill"); Tick();
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech == std::vector<std::string>{"Nueva descripción."},
		"a skill without prerequisites does not inherit the previous skill's requirements");
	ClearSpeech(); progression = {}; widgetType = {"TabWidget", UiText::Kind::TabButton}; focus.widget = 73;
	buttonSnapshot = {73, 73, "Tab1", UiText::Surface::Other, {{73, -1, UiText::Kind::TabButton, "Tab1", "", "Dominio del combate"}}};
	Check(InteractionReader::HandlesFocus(focus) && Tick() && speech == std::vector<std::string>{"Dominio del combate"}, "focused branch tabs are readable through the shared interaction pipeline");
	InteractionReader::Reset(); ClearSpeech(); progression = {}; tooltip = {}; controllerHelp.clear();
	focus = {20, 81}; widgetType = {"InventorySlotWidget", UiText::Kind::InventorySlot};
	cell = {}; equipmentLocation = 1; equipmentPresence = NativeUi::ItemPresence::Empty;
	Check(Tick() && speech == std::vector<std::string>{"Inventario. Equipo. Cabeza. Vacío"}, "empty equipment announces its location and presence on the first poll");
	ClearSpeech(); focus.widget = 82; equipmentLocation = 2; equipmentPresence = NativeUi::ItemPresence::Occupied;
	Check(Tick() && speech.empty(), "an occupied slot waits for its item's name instead of saying it is occupied");
	ClearSpeech(); Tooltip("Resistencia al fuego: 10%\nAmuleto"); Tick();
	Check(speech == std::vector<std::string>{"Cuello. Amuleto"}, "delayed equipped item retains the slot name with the item's name");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Resistencia al fuego: 10%"}, "an equipped item's properties are there for the asking");
	ClearSpeech(); equipmentPresence = NativeUi::ItemPresence::Empty; Tick();
	Check(speech == std::vector<std::string>{"Cuello. Vacío"}, "removing equipment rejects a lingering tooltip even without changing focus");
	ClearSpeech(); equipmentPresence = NativeUi::ItemPresence::Occupied; tooltip = {}; Tick();
	Check(speech.empty(), "equipping into the focused slot waits for the item's name too");
	ClearSpeech(); Tick(); Check(speech.empty(), "stationary occupied slot without a description does not repeat");
	Tooltip("Resistencia al fuego: 10%\nAmuleto"); Tick();
	Check(speech == std::vector<std::string>{"Cuello. Amuleto"}, "and the slot is then read once, with its item");
	InteractionReader::Reset(); ClearSpeech(); equipmentPresence = NativeUi::ItemPresence::Unknown;
	focus = {20, 21}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid}; selection = 0; cell = {0, 0};
	itemPresence = NativeUi::ItemPresence::Occupied;
	controllerHelp = "Y: Reparar todo. B: Cerrar menú";
	Check(Tick() && speech.empty(), "available actions wait for the selected item's description instead of being read ahead of it");
	ClearSpeech(); Tick(); Check(speech.empty(), "unchanged merchant actions do not repeat while waiting");
	Tooltip("Durabilidad: 15 de 28\nHacha"); Tick();
	Check(speech == std::vector<std::string>{"Inventario. Hacha"},
		"with autodetail off the item is read alone: the buttons are never spoken unasked");
	Check(InteractionReader::Repeat().find("Reparar") == std::string::npos, "nor repeated unasked");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Down;
	Check(Tick() && speech == std::vector<std::string>{controllerHelp}, "right stick down reads them");
	// With autodetail and control tooltips on they follow the item, last and
	// queued -- never interrupting what they follow, as they cut off the gold
	// amount (MEASURED 2026-09-21).
	autoDetail = true;
	ClearSpeech(); selection = 1; cell = {1, 0}; Tooltip("Espada"); Tick();
	Check(speech == std::vector<std::string>{"Espada"}, "item-to-item navigation does not repeat unchanged merchant actions");
	ClearSpeech(); controllerHelp = "X: Actualizar. B: Cerrar menú"; Tick();
	Check(speech == std::vector<std::string>{controllerHelp}, "a new NPC action is read without repeating the selected item");
	Check(InteractionReader::Repeat().ends_with(controllerHelp), "and with autodetail on, repeat includes the controls");
	controlTooltips = false;
	ClearSpeech(); controllerHelp = "B: Cerrar menú"; Tick();
	Check(speech.empty(), "with control tooltips off, autodetail leaves the buttons to right stick down");
	controlTooltips = true; autoDetail = false;
	ClearSpeech(); conversation = true; Tick();
	Check(speech.empty() && InteractionReader::Repeat().empty(), "NPC dialogue suppresses inventory actions underneath it");
	conversation = false;
	// With the pick-up and placement speech switched off, nothing is announced
	// for either, in any section of the inventory (the collaborator's player,
	// 2026-09-21: the notice came back on every move to the belt, the
	// equipment or a shop, and "it needs to go completely").
	InteractionReader::SetHandSpeech(false);
	InteractionReader::Reset(); ClearSpeech(); controllerHelp.clear(); tooltip = {};
	focus = {20, 21}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid};
	holdingItem = false; itemPresence = NativeUi::ItemPresence::Occupied;
	Tick(); holdingItem = true; Tick();
	Check(speech.empty(), "picking up an item announces nothing");
	focus.widget = 81; widgetType = {"InventorySlotWidget", UiText::Kind::InventorySlot}; equipmentLocation = 1;
	cell = {};
	equipmentPresence = NativeUi::ItemPresence::Empty; Tick();
	Check(speech == std::vector<std::string>{"Inventario. Equipo. Cabeza. Vacío"}, "crossing into equipment with an item held reads only the slot");
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
	Check(InteractionReader::Repeat() == "Inventario. Equipo. Cabeza. Vacío", "manual repeat is the readout alone");
	ClearSpeech(); holdingItem = false; Tick();
	Check(speech.empty(), "putting an item down announces nothing either");
	// Switched on, the default, the hand is told.
	InteractionReader::SetHandSpeech(true);
	ClearSpeech(); holdingItem = true; Tick();
	Check(!speech.empty() && speech.front() == Strings::Current().itemInHand, "with the switch on, picking up says the item is in hand");
	Check(InteractionReader::Repeat().starts_with(Strings::Current().itemInHand), "and a manual repeat says so first");
	ClearSpeech(); holdingItem = false; Tick();
	Check(std::find(speech.begin(), speech.end(), std::string("Colocado en Cabeza")) != speech.end(),
		"putting it down names where it went");
	InteractionReader::SetHandSpeech(false);
	InteractionReader::Reset(); ClearSpeech();
	focus = {20, 21}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid};
	cell = {0, 0}; selection = 0; holdingItem = false; itemPresence = NativeUi::ItemPresence::Empty;
	Settle(); ClearSpeech(); holdingItem = true; cell = {1, 0}; selection = 1; Settle();
	Check(speech == std::vector<std::string>{"Fila 1, columna 2"}, "with an item held, an empty cell reads its position as always");
	// With an item held the game draws no tooltip (MEASURED 2026-09-21), so the
	// item under the cursor is named from the grid or the slot instead.
	ClearSpeech(); itemPresence = NativeUi::ItemPresence::Occupied; gridItem = 77; cell = {2, 0}; selection = 2; Settle();
	Check(speech == std::vector<std::string>{"Poción de curación menor"}, "a held item over another names the one beneath it");
	ClearSpeech(); cell = {2, 1}; selection = (uint64_t{1} << 32) | 2; Settle();
	Check(speech == std::vector<std::string>{"Poción de curación menor"}, "on every cell that item covers");
	ClearSpeech(); gridItem = 55; cell = {3, 0}; selection = 3; Settle();
	Check(speech == std::vector<std::string>{Strings::Current().slotOccupied}, "an item it cannot name is still said to be there");
	ClearSpeech(); focus.widget = 81; widgetType = {"InventorySlotWidget", UiText::Kind::InventorySlot}; cell = {};
	equipmentLocation = 4; equipmentPresence = NativeUi::ItemPresence::Occupied; equipmentItem = 77; Settle();
	Check(speech == std::vector<std::string>{"Inventario. Equipo. Arma derecha. Poción de curación menor"},
		"an equipment slot names what it holds instead of only that it is occupied");
	ClearSpeech(); holdingItem = false; focus.widget = 21; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid};
	gridItem = 77; cell = {4, 0}; selection = 4; Settle();
	Check(speech.empty(), "with nothing held an occupied cell still waits for the game's own tooltip");
	gridItem = 0; equipmentItem = 0; equipmentPresence = NativeUi::ItemPresence::Unknown; equipmentLocation = 0;
	// An item's size, the cells it takes up: on its own on right stick right,
	// never in the detail on right stick up (the user, 2026-09-21), and right
	// after the name with autodetail on. Each cell the item covers names it.
	InteractionReader::Reset(); ClearSpeech(); holdingItem = false; tooltip = {};
	focus = {20, 21}; widgetType = {"InventoryGridWidget", UiText::Kind::InventoryGrid};
	itemPresence = NativeUi::ItemPresence::Occupied; gridItem = 55; cell = {7, 0}; selection = 7;
	Tooltip("Daño: 2 a 7\nEspada corta"); Settle();
	Check(speech == std::vector<std::string>{"Inventario. Espada corta"}, "with autodetail off an item reads its name alone");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Daño: 2 a 7"}, "right stick up reads the detail without the size");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Right;
	Check(Tick() && speech == std::vector<std::string>{"1 columna por 3 filas"}, "right stick right reads the cells it takes up");
	ClearSpeech(); tooltip = {}; cell = {7, 1}; selection = (uint64_t{1} << 32) | 7; Settle();
	Tooltip("Daño: 2 a 7\nEspada corta"); Settle();
	Check(speech == std::vector<std::string>{"Espada corta"}, "the next cell the item covers names it again");
	autoDetail = true; ClearSpeech(); tooltip = {}; cell = {7, 2}; selection = (uint64_t{2} << 32) | 7; Settle();
	Tooltip("Daño: 2 a 7\nEspada corta\nValor de venta: 71"); Settle();
	Check(speech == std::vector<std::string>{"Espada corta. 1 columna por 3 filas. Valor de venta: 71. Daño: 2 a 7"},
		"with autodetail on the size comes right after the name, ahead of the price");
	ClearSpeech(); tooltip = {}; holdingItem = true; gridItem = 77; cell = {0, 3}; selection = uint64_t{3} << 32; Settle();
	Check(speech == std::vector<std::string>{"Poción de curación menor. 1 columna por 1 fila"}, "an item under a held one gives its size too");
	autoDetail = false; ClearSpeech(); pendingPush = InputDirection::StickPush::Right;
	Check(Tick() && speech == std::vector<std::string>{"1 columna por 1 fila"}, "which right stick right reads when autodetail is off");
	ClearSpeech(); holdingItem = false; gridItem = 56; cell = {1, 3}; selection = (uint64_t{3} << 32) | 1;
	Tooltip("Objeto desconocido"); Settle(); pendingPush = InputDirection::StickPush::Right; ClearSpeech();
	Check(Tick() && speech == std::vector<std::string>{Strings::Current().detailNone}, "an item the tables do not know gives no size");
	gridItem = 0; tooltip = {};
	// Compare mode, while Y is held: the left stick reads -- left the highlighted
	// item, right the equipped one the game draws beside it, up the detail and
	// down the controls -- and the right stick is unchanged.
	Check(!InteractionReader::ItemLogging(), "item tooltips are not logged unless asked for");
	ClearSpeech(); holdingItem = false; itemPresence = NativeUi::ItemPresence::Occupied;
	cell = {2, 3}; selection = (uint64_t{3} << 32) | 2;
	Tooltip("Defensa: 2\nGuantes"); Settle();
	Check(speech == std::vector<std::string>{"Guantes"}, "the item reads as usual");
	Check(PadFilter::leftStickArms > 0, "an inventory item arms the left stick for a held Y");
	ClearSpeech(); tooltip.extras = {"Defensa: 3\nGuantes de cuero\nActualmente equipado"}; InputDirection::holdingY = true; Settle();
	Check(speech.empty(), "holding compare reads nothing by itself");
	// With compare-aloud on, the equipped item is read as the game draws it (a tester, 2026-10-13).
	InteractionReader::SetCompareAloud(true);
	tooltip.extras.clear(); Settle();
	ClearSpeech(); tooltip.extras = {"Defensa: 3\nGuantes de cuero"}; Settle();
	Check(speech == std::vector<std::string>{"Actualmente equipado. Guantes de cuero. Defensa: 3"},
		"with compare-aloud on, the equipped item is read as it appears, under the game's own heading");
	ClearSpeech(); Settle();
	Check(speech.empty(), "and once");
	tooltip.extras.clear(); Settle();
	ClearSpeech(); tooltip.extras = {"Defensa: 3\nGuantes de cuero\nActualmente equipado"}; Settle();
	Check(speech == std::vector<std::string>{"Actualmente equipado. Guantes de cuero. Defensa: 3"}, "a heading already drawn is not said twice");
	InteractionReader::SetCompareAloud(false); ClearSpeech();
	ClearSpeech(); InputDirection::pendingLeft = InputDirection::StickPush::Right;
	Check(Tick() && speech == std::vector<std::string>{"Actualmente equipado. Guantes de cuero. Defensa: 3"}, "left stick right reads the equipped item");
	ClearSpeech(); InputDirection::pendingLeft = InputDirection::StickPush::Left;
	Check(Tick() && speech == std::vector<std::string>{"Guantes. Defensa: 2"}, "left stick left reads the highlighted item in full");
	ClearSpeech(); InputDirection::pendingLeft = InputDirection::StickPush::Up;
	Check(Tick() && speech == std::vector<std::string>{"Defensa: 2"}, "left stick up reads the detail, as the right stick would");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Right;
	Check(Tick() && speech == std::vector<std::string>{Strings::Current().detailNone}, "the right stick is unchanged while Y is held: right is the size");
	ClearSpeech(); tooltip.extras.clear(); InputDirection::pendingLeft = InputDirection::StickPush::Right;
	Check(Tick() && speech == std::vector<std::string>{Strings::Current().detailNone}, "with nothing equipped to compare, it says so");
	InputDirection::holdingY = false;
	tooltip = {}; itemPresence = NativeUi::ItemPresence::Empty;
	// The tab bar across the top of the menus reads the tab its highlight is on.
	ClearSpeech(); focus = {20, 95}; widgetType = {"TabBarWidget", UiText::Kind::Tabs}; selection = 6;
	buttonSnapshot = {95, 95, "UISwitcherTabs", UiText::Surface::Other, {{95, -1, UiText::Kind::Tabs, "UISwitcherTabs", "TabBarWidget", "Inventario"}}};
	Check(InteractionReader::HandlesFocus(focus), "the tab bar is read by the interaction reader");
	Settle();
	Check(speech == std::vector<std::string>{"Opciones"}, "the highlighted tab is read, not the open one");
	ClearSpeech(); selection = 5; Settle();
	Check(speech == std::vector<std::string>{"Mercenario"}, "moving along the bar reads each tab");
	ClearSpeech(); focus.widget = 28; widgetType = {"ButtonWidget", UiText::Kind::Button};
	buttonSnapshot = {28, 28, "OtherAction", UiText::Surface::Other, {{28, -1, UiText::Kind::Button, "OtherAction", "ButtonWidget", "Identificar"}}};
	Settle(); ClearSpeech(); pendingPush = InputDirection::StickPush::Right; Tick();
	Check(pendingPush == InputDirection::StickPush::Right, "right stick right is claimed only on the inventory");
	pendingPush = InputDirection::StickPush::None;

	// The branch leads with the skill choices left as the tree opens and on LT/RT,
	// queued behind the window's title on opening (a tester, 2026-10-13).
	InteractionReader::Reset(); ClearSpeech(); interrupts.clear(); progression = {}; tooltip = {};
	focus = {75, 76}; widgetType = {"ButtonWidget", UiText::Kind::Button}; buttonSnapshot = {};
	progression.skill = true; progression.classTree = true; progression.tab = 500; progression.points = "1 punto";
	progression.readout = {"Skills", "Gritos de guerra", "", "76"};
	Tooltip("\xEE\x81\xBE" "2Aullido"); Tick();
	Check(speech == std::vector<std::string>{"Gritos de guerra. 1 punto", "Aullido"} && interrupts == std::vector<bool>{false, false},
		"an opened tree names its branch and the choices left, queued, before its first skill");
	ClearSpeech(); interrupts.clear();
	focus.widget = 77; progression.tab = 501; progression.readout = {"Skills", "Maestrías", "", "77"};
	Tooltip("\xEE\x81\xBE" "2Golpe"); Tick();
	Check(speech == std::vector<std::string>{"Maestrías. 1 punto", "Golpe"} && interrupts == std::vector<bool>{true, false},
		"LT/RT to another tree names it with the choices left, as a change of context");
	ClearSpeech(); interrupts.clear();
	focus.widget = 78; progression.tab = 502; progression.classTree = false; progression.readout = {"Skills", "Habilidades generales", "", "78"};
	Tooltip("\xEE\x81\xBE" "2Portal"); Tick();
	Check(speech == std::vector<std::string>{"Habilidades generales", "Portal"}, "General Skills is named without the class's choices");
	ClearSpeech(); interrupts.clear();
	focus.widget = 79; progression.tab = 501; progression.classTree = true; progression.points = "0 puntos";
	progression.readout = {"Skills", "Maestrías", "", "79"};
	Tooltip("\xEE\x81\xBE" "2Golpe"); Tick();
	Check(speech == std::vector<std::string>{"Maestrías", "Golpe"}, "with no choices left the branch is said alone");
	ClearSpeech(); interrupts.clear();
	focus = {}; Tick(); focus = {75, 79}; Tooltip("\xEE\x81\xBE" "2Golpe"); Tick();
	Check(!speech.empty() && speech.front() == "Maestrías" && !interrupts.front(), "a tree reopened on the same branch names it again");

    InteractionReader::Reset(); progression = {}; tooltip = {}; buttonSnapshot = {};
    focus = {20, 100}; widgetType = {"SkillButtonWidget", UiText::Kind::Button};
    progression.skill = true; progression.name = "Salto"; progression.readout.focusKey = "100";
    ClearSpeech(); Settle();
    Check(speech.size() == 1 && speech.front().find("Salto") != std::string::npos,
        "a verified skill-tree table name speaks before its tooltip exists");
    ClearSpeech(); Tooltip("Salto"); Tick();
    Check(speech.empty(), "a matching late tooltip does not repeat a skill name already spoken");
    progression = {}; InteractionReader::Reset();
	InteractionReader::Reset(); ClearSpeech(); inventory.nodes.clear(); tooltip = {};
	InputDirection::holdingY = false; autoDetail = false; holdingItem = false; controllerHelp.clear();
	cell = {}; selection = 0; focus = {10, 201}; widgetType = {"AdvancedStashSlotWidget", UiText::Kind::InventoryItem};
	stashItem = {true, 77};
	Check(Tick() && speech == std::vector<std::string>{"Runa Tal"},
		"a rune slot is named in its first poll without waiting for a tooltip or cursor settle");
	ClearSpeech(); Tooltip("Daño: 5\nRuna Tal"); Tick();
	Check(speech.empty(), "the matching rune tooltip supplies detail without repeating its already spoken name");
	ClearSpeech(); pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech == std::vector<std::string>{"Daño: 5"}, "rune properties remain available through the existing detail command");
	ClearSpeech(); focus.widget = 202; stashItem = {true, 0}; Tooltip("Daño: 5\nRuna Tal");
	Check(Tick() && speech == std::vector<std::string>{"Diamante fragmentado. " + std::string(Strings::Current().slotEmpty)},
		"an empty gem slot says its localized identity immediately and rejects the previous rune tooltip");
	ClearSpeech(); for (int i = 0; i < 6; ++i) Tick();
	Check(speech.empty(), "a fixed stash slot never accepts another item's tooltip after the old mismatch timeout");
	pendingPush = InputDirection::StickPush::Up; Tick();
	Check(speech == std::vector<std::string>{Strings::Current().detailNone}, "moving to a gem drops the previous rune's detail");
	ClearSpeech(); stashItem = {true, 77}; Tooltip("Diamante fragmentado"); Tick();
	Check(speech == std::vector<std::string>{"Diamante fragmentado"}, "a newly occupied gem slot updates without another settle delay");
	ClearSpeech(); focus.widget = 201; stashItem = {}; tooltip = {};
	Check(Tick() && speech == std::vector<std::string>{"Runa Tal"}, "an unavailable slot getter still names the visible fixed rune without claiming it is empty");
	InteractionReader::Reset();
    Passed();
}
