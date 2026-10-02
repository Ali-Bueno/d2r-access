#include "async_log.h"
#include "auto_detail.h"
#include "character_stats.h"
#include "character_stats_reader.h"
#include "entity_cue_service.h"
#include "entity_radar_service.h"
#include "equip_attempt.h"
#include "interaction_reader.h"
#include "belt_reader.h"
#include "inventory_cursor.h"
#include "ground_item_service.h"
#include "ground_item_text.h"
#include "input_direction.h"
#include "item_hand.h"
#include "localization.h"
#include "monster_names.h"
#include "native_tooltips.h"
#include "native_controller_help.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_progression.h"
#include "progression_text.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "tab_reader.h"
#include "tooltip_text.h"
#include "ui_text.h"

#include <D2RLPlugin/item.h>
#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace D2RAccess::InteractionReader {
namespace {
UiText::Reader g_reader;
bool g_inventory = false;
constexpr uint64_t InventorySettleMs = 20; // at most one subsequent 30 Hz UI poll
struct InventoryContext {
	uintptr_t panel = 0;
	std::string name, title;
};
InventoryContext g_inventoryContext;
struct StatValue { uintptr_t widget = 0; std::string value, points; };
StatValue g_stat;
std::string g_help, g_spokenHelp, g_helpContext;
// The focus the last real readout described, so a hint can tell "nothing new
// to say here" from "nothing said about here yet".
std::string g_describedFocus;
// The world label last dropped for being out of sight, so the log records each
// one once instead of on every UI poll.
std::string g_hiddenLabel, g_quietMonster;
MonsterNames::Gate g_monsterNames;
// The game's monster health plate: the panel the auto-target names its monster in.
constexpr std::string_view MonsterPlate = "HUDMonsterHealth";
std::optional<bool> g_holdingItem;
// Red requirement lines of the item last described, and of the one in hand,
// taken when it was picked up: why the game would refuse to equip it.
std::string g_itemUnmet, g_heldUnmet;
EquipAttempt::Tracker g_equipAttempt;
// The runtime id in hand, and what the focus rested on while the hand was full:
// the item a gem goes into or the game swaps back. docs/inventory-npc-followup.md#socketing
uint32_t g_heldId = 0;
bool g_heldFiller = false; // a gem, rune or jewel is in hand
struct Hovered { ItemHand::Under item; std::string name; bool read = false; };
Hovered g_hovered;
struct SocketRead { uint32_t id = 0; std::string state; };
SocketRead g_socketRead;
ItemHand::Names g_itemNames;
// An equipment slot's location never changes, but the native read misses it
// while the game rebuilds the panel, and the label would then simply vanish.
// Widget names are stable across those rebuilds, so the last value seen under
// a name stands in for a failed read.
struct KnownSlot { std::string name; int location = 0; bool reported = false; };
std::vector<KnownSlot> g_slotLocations;

// The part of a description the player asked not to hear on every move: an
// item's requirements, properties, flavour text and price. Held for whatever
// the highlight is on now, and read on demand with a push up on the right
// stick while a panel is open.
std::string g_detail, g_detailFor;
std::string g_comparison, g_comparisonFor;
std::string g_rowDetail, g_rowDetailFor;
std::vector<std::string> g_rowTooltips;

// The cell whose item was named, and the cell and headline its tooltip was
// logged under, so [item] is written once per item instead of on every UI poll
// while a tooltip is on screen (each line is an open, write and flush --
// async_log.cpp).
std::string g_namedFocus, g_loggedTooltip;

// The game's own labels for a set of string keys, as the part of each printf
// format before its first placeholder -- the only part that can begin a
// rendered line. Resolved again whenever the text language changes: the main
// menu switches it without restarting the game.
class Labels {
public:
	template <typename Keys> const std::vector<std::string>& For(const Keys& keys) {
		if (language_ != Strings::Selected()) {
			language_ = Strings::Selected();
			prefixes_.clear();
			for (const char* key : keys)
				if (auto text = TooltipText::LabelPrefix(Localization::Text(key)); !text.empty()) prefixes_.push_back(std::move(text));
		}
		return prefixes_;
	}

private:
	std::optional<Strings::Language> language_;
	std::vector<std::string> prefixes_;
};

// A weapon's damage and an armour's defense, so the split can tell them from the name.
const std::vector<std::string>& StatLabels() {
	static Labels cache;
	return cache.For(TooltipText::BaseStatKeys);
}

// Requirement labels, whose red lines say what the character cannot meet.
const std::vector<std::string>& RequirementLabels() {
	static Labels cache;
	return cache.For(TooltipText::RequirementKeys);
}

// A tooltip body with any magic name the game composed with a broken grammar
// header rebuilt, the format resolved again when the text language changes.
TooltipText::Body ItemBody(std::string_view raw) {
	static std::optional<Strings::Language> language;
	static std::string format;
	if (language != Strings::Selected() || format.empty()) {
		language = Strings::Selected();
		format = Localization::Text(GroundItemText::SuffixOnlyFormatKey);
	}
	auto body = TooltipText::Read(raw);
	for (auto& line : body.lines) line = GroundItemText::RepairSuffixOnlyName(line, format);
	return body;
}

std::string Refusal(const std::string& reasons) {
	if (reasons.empty()) return Strings::Current().equipRefused;
	char phrase[300]{};
	std::snprintf(phrase, sizeof(phrase), Strings::Current().equipRefusedBecause, reasons.c_str());
	return phrase;
}

// Price labels distinguish the item's name from a shop tooltip's first line.
const std::vector<std::string>& PriceLabels() {
	static constexpr const char* Keys[] = {TooltipText::PriceKey, TooltipText::SellKey};
	static Labels cache;
	return cache.For(Keys);
}

// A push up on the right stick, claimed only while a panel is open so the
// level scanner keeps the stick during play.
bool ReadDetail(const D2RL::PluginContext* context, NativeUi::Focus focus) {
	if (!focus.panel || !InputDirection::TakeRightStickPushIf(InputDirection::StickPush::Up, "detail")) return false;
	const std::string& text = g_detail.empty() ? Strings::Current().detailNone : g_detail;
	D2RAccess::AsyncLog::InfoF(context, "[detail] %s -> \"%s\"", g_detailFor.c_str(), text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

int SlotLocation(const D2RL::PluginContext* context, uintptr_t widget) {
	const int live = NativeUi::EquipmentLocation(widget);
	const auto name = NativeWidgets::ReadName(widget);
	if (name.empty()) return live;
	for (auto& known : g_slotLocations) {
		if (known.name != name) continue;
		if (live > 0) { known.location = live; known.reported = false; return live; }
		if (!known.reported) {
			known.reported = true;
			AsyncLog::InfoF(context, "[interaction] equipment location unreadable for %s; keeping %d",
				name.c_str(), known.location);
		}
		return known.location;
	}
	if (live > 0) g_slotLocations.push_back({name, live, false});
	return live;
}

// The label of the equipment slot a location belongs to, null when unknown.
const char* SlotName(int location) {
	const auto& labels = Strings::Current().equipmentSlots;
	return location > 0 && static_cast<size_t>(location) < labels.size() ? labels[location] : nullptr;
}

void Append(std::string& text, const std::string& part) {
	if (part.empty() || text == part) return;
	if (!text.empty()) text += text.ends_with('.') ? " " : ". ";
	text += part;
}

// Comparison records contain the equipped items the game actually drew, in
// its own order (including its selected weapon/ring when cycling comparisons).
std::string ComparisonText(const NativeTooltips::Snapshot& tooltip,
	const TooltipText::Description& selected) {
	if (tooltip.comparisons.empty()) return {};
	std::string equipped;
	const auto heading = UiText::Clean(Localization::Text("currentEquip"));
	for (const auto& record : tooltip.comparisons) {
		const auto body = ItemBody(record.body);
		std::string text;
		for (const auto& line : body.lines) if (line != heading) Append(text, line);
		if (text.empty()) continue; // decorative controller glyphs are not items
		Append(equipped, heading);
		Append(equipped, text);
		for (const auto& line : TooltipText::Read(record.footer).lines)
			if (line != heading) Append(equipped, line);
	}
	if (equipped.empty()) return {};
	std::string result = selected.headline;
	Append(result, selected.detail);
	Append(result, equipped);
	return result;
}

std::vector<std::string> RowTooltips(const NativeTooltips::Snapshot& tooltip) {
	std::vector<std::string> result;
	const auto appendRecord = [&](const auto& record) {
		std::string text;
		for (const auto& line : TooltipText::Read(record.body).lines) Append(text, line);
		for (const auto& line : TooltipText::Read(record.footer).lines) Append(text, line);
		if (!text.empty()) result.push_back(std::move(text));
	};
	appendRecord(tooltip);
	for (const auto& record : tooltip.comparisons) appendRecord(record);
	return result;
}

// Keep only the tooltip text that has not already been announced.
struct TooltipMerge { std::string addition; bool corrects = false; };
TooltipMerge MergeTooltip(const std::string& name, std::string description) {
	if (name.empty()) return {std::move(description), true};
	if (description.empty() || name.starts_with(description)) return {};
	if (!description.starts_with(name)) return {std::move(description), true};
	description.erase(0, name.size());
	description.erase(0, description.find_first_not_of(". "));
	return {std::move(description), false};
}

// The inventory's gold row, named by the game's own widget name.
constexpr std::string_view GoldButton = "gold_button";
// The stash's withdraw button and the text box beside it that shows the open tab's gold.
constexpr std::string_view StashGoldButton = "gold_withdraw";
constexpr std::string_view StashGoldBox = "gold_amount", StashGoldContainer = "basicstash_container";

// The digits of the stash's gold box, "" when it cannot be read.
std::string StashGold(const D2RL::PluginContext* context, uintptr_t panel) {
	uintptr_t box = NativeWidgets::FindChild(panel, StashGoldBox);
	if (!box) box = NativeWidgets::FindChild(NativeWidgets::FindChild(panel, StashGoldContainer), StashGoldBox);
	const std::string text = UiText::Clean(NativeWidgets::ReadText(box));
	std::string digits;
	for (const char c : text)
		if (c >= '0' && c <= '9') digits += c;
	static std::string logged;
	if (text != logged) {
		logged = text;
		AsyncLog::InfoF(context, "[stash] gold box=%d text=\"%s\" stat=%d", box ? 1 : 0, text.c_str(), NativeGroundItems::PlayerStashGold());
	}
	return digits;
}

bool IsInventory(UiText::Kind kind) {
	return kind == UiText::Kind::InventoryGrid || kind == UiText::Kind::InventorySlot || kind == UiText::Kind::InventoryItem;
}

// The trade window's two grids and the partner's name box (TradeLayoutHD.json).
constexpr std::string_view TradeGridUs = "grid_us", TradeGridThem = "grid_them", TradeNameThem = "name_them";

std::string TradePartner(uintptr_t panel) {
	return UiText::Clean(NativeWidgets::ReadText(NativeWidgets::FindChild(panel, TradeNameThem)));
}

std::string Formatted(const char* format, const std::string& value) {
	char phrase[160]{};
	std::snprintf(phrase, sizeof(phrase), format, value.c_str());
	return phrase;
}

std::string Title(uintptr_t panel) {
	if (NativeUi::Classify(panel).name == "TradePanel") {
		const auto partner = TradePartner(panel);
		return partner.empty() ? Localization::Text("NPCMenuTrade") : Formatted(Strings::Current().tradeWith, partner);
	}
	const bool stash = NativeUi::Classify(panel).name == "BankPanel";
	const bool hireling = NativeWidgets::ReadName(panel) == "HirelingInventoryPanel";
	const auto localized = Localization::Text(stash ? "stash" : hireling ? "MiniPanelHireinv" : "minipanelinv");
	return localized.empty() ? (stash ? Strings::Current().stash : Strings::Current().inventory) : localized;
}

UiText::Readout Inventory(const D2RL::PluginContext* context, NativeUi::Focus focus, const NativeUi::Type& type) {
	if (g_inventoryContext.panel != focus.panel) {
		g_inventoryContext = {focus.panel, NativeWidgets::ReadName(focus.panel), Title(focus.panel)};
	}
	UiText::Readout result;
	result.context = g_inventoryContext.name;
	result.body = g_inventoryContext.title;
	Append(result.body, TabReader::Selected(focus.panel));
	if (type.kind == UiText::Kind::InventorySlot) Append(result.body, Strings::Current().equipment);
	else if (type.name == "BeltWidget") Append(result.body, Strings::Current().belt);
	else if (const auto grid = NativeWidgets::ReadName(focus.widget); grid == TradeGridUs) Append(result.body, Strings::Current().tradeYourOffer);
	else if (grid == TradeGridThem) {
		const auto partner = TradePartner(focus.panel);
		Append(result.body, Formatted(Strings::Current().tradeTheirOffer, partner.empty() ? Strings::Current().tradePartnerUnknown : partner));
	}
	result.focusKey = std::to_string(focus.widget) + ":" + std::to_string(NativeUi::FocusSelection(focus.widget));
	const auto cell = NativeUi::GridCell(focus.widget);
	if (cell.x >= 0) {
		char phrase[100]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().gridCell, cell.y + 1, cell.x + 1);
		result.coordinates = phrase;
	}
	// The section in the body already says "equipment"; only a real slot name
	// is worth adding here. A slot with nothing else to say is covered in Tick.
	if (type.kind == UiText::Kind::InventorySlot)
		if (const char* slot = SlotName(SlotLocation(context, focus.widget))) Append(result.option, slot);
	return result;
}

// Where a carried item was just put down, so releasing it is not only "item
// released". Equipment slots and grid cells are exact; the belt is one widget,
// so only the section is known.
std::string Destination(const D2RL::PluginContext* context, NativeUi::Focus focus, const NativeUi::Type& type) {
	if (type.kind == UiText::Kind::InventorySlot) {
		const char* slot = SlotName(SlotLocation(context, focus.widget));
		return slot ? slot : Strings::Current().equipment;
	}
	if (type.name == "BeltWidget") {
		const auto slot = NativeUi::FocusedItem(focus.widget);
		const auto location = slot.known ? BeltReader::Location(slot.slot) : std::string{};
		return location.empty() ? Strings::Current().belt : location;
	}
	const auto cell = NativeUi::GridCell(focus.widget);
	if (cell.x < 0) return {};
	char phrase[100]{};
	std::snprintf(phrase, sizeof(phrase), Strings::Current().gridCell, cell.y + 1, cell.x + 1);
	return phrase;
}

// A table name with its quality: what an item is called when no tooltip names it.
std::string TableName(const std::string& base, uint32_t quality) {
	const auto& qualities = Strings::Current().itemQualities;
	std::string name = base;
	if (quality < qualities.size()) Append(name, qualities[quality]);
	return name;
}

// The item the game handed back: the one just under the focus, or one of its own.
std::string HeldName(const InventoryCursor::ItemState& held) {
	if (held.id == g_hovered.item.id && !g_hovered.name.empty()) return g_hovered.name;
	return ItemName(held.id, held.classId, held.quality);
}

ItemHand::Under UnderOf(const InventoryCursor::ItemState& item) { return {item.id, item.sockets, item.filled, item.quantity}; }

// An item's sockets: the total from the SDK, the filled ones from the item's own
// inventory, because the SDK read 0 for a filled socket. docs/inventory-npc-followup.md#socket-contents
std::optional<ItemHand::Under> ReadUnder(const D2RL::PluginContext* context, uint32_t id, uintptr_t unit) {
	const auto state = id ? InventoryCursor::Find(context, id) : std::nullopt;
	if (!state) return std::nullopt;
	auto item = UnderOf(*state);
	item.filled = NativeEntities::FilledSockets(unit).value_or(ItemHand::UnknownFilled);
	return item;
}

// What the focus rests on while the hand is full, read once per item.
void Hover(const D2RL::PluginContext* context, uint32_t id, uintptr_t unit, const std::string& name) {
	if (id == g_hovered.item.id) {
		if (!name.empty()) g_hovered.name = name;
		return;
	}
	g_hovered = {{id}, name};
	if (const auto item = ReadUnder(context, id, unit)) {
		g_hovered.item = *item;
		g_hovered.read = true;
	}
}

// Whether an item has room for a gem, rune or jewel; only the total when the
// filled ones cannot be read.
std::string SocketState(const ItemHand::Under& item) {
	const auto& strings = Strings::Current();
	if (!item.sockets) return strings.socketsNone;
	char text[80]{};
	if (item.filled == ItemHand::UnknownFilled)
		std::snprintf(text, sizeof(text), strings.socketsCount, static_cast<int>(item.sockets));
	else if (item.filled >= item.sockets) return strings.socketsFull;
	else std::snprintf(text, sizeof(text), strings.socketsFree, static_cast<int>(item.sockets - item.filled));
	return text;
}

// What the hand did: picked up, put down, socketed, stacked, swapped or thrown.
std::string HandNotice(const D2RL::PluginContext* context, NativeUi::Focus focus, const NativeUi::Type& type,
	const InventoryCursor::ItemState& held, uint32_t heldBefore, bool wasHolding, bool filler, uint32_t focusedId, uintptr_t focusedUnit) {
	const auto& strings = Strings::Current();
	if (held.id && !wasHolding) {
		g_heldUnmet = g_itemUnmet;
		return strings.itemInHand;
	}
	if (!wasHolding) return {};
	ItemHand::Under after{focusedId};
	if (focusedId && focusedId == g_hovered.item.id)
		if (const auto item = ReadUnder(context, focusedId, focusedUnit)) after = *item;
	const bool dropPressed = InputDirection::LastFaceButton(EquipAttempt::AnswerMs) == InputDirection::FaceButton::X;
	const auto outcome = ItemHand::Resolve(heldBefore, held.id, g_hovered.item, after, filler, dropPressed);
	if (outcome != ItemHand::Outcome::Placed && outcome != ItemHand::Outcome::None)
		AsyncLog::InfoF(context, "[item] hand outcome=%d held=%u->%u filler=%u under=%u sockets=%u/%u->%u/%u quantity=%d->%d name=\"%s\"",
			static_cast<int>(outcome), heldBefore, held.id, filler ? 1u : 0u, g_hovered.item.id, g_hovered.item.filled,
			g_hovered.item.sockets, after.filled, after.sockets, g_hovered.item.quantity, after.quantity, g_hovered.name.c_str());
	switch (outcome) {
	case ItemHand::Outcome::Swapped: {
		// A gem handed back for the item it was put on: the game would not socket it.
		std::string text = filler && held.id == g_hovered.item.id ? strings.socketRefused : "";
		Append(text, Formatted(strings.itemSwapped, HeldName(held)));
		return text;
	}
	case ItemHand::Outcome::Merged: return Formatted(strings.itemAddedTo, g_hovered.name);
	case ItemHand::Outcome::Thrown: return strings.itemDropped;
	case ItemHand::Outcome::Socketed: {
		auto text = Formatted(strings.itemSocketedIn, g_hovered.name);
		Append(text, SocketState(after));
		return text;
	}
	case ItemHand::Outcome::Placed: case ItemHand::Outcome::None: break;
	}
	if (held.id) return {};
	const auto destination = Destination(context, focus, type);
	return destination.empty() ? std::string(strings.itemReleased) : Formatted(strings.itemPlacedIn, destination);
}

// The sockets of the item under a gem in hand, read once per item and hand.
std::string SocketsUnder(const D2RL::PluginContext* context, uint32_t id, uintptr_t unit) {
	if (id != g_socketRead.id) {
		g_socketRead = {id, {}};
		if (const auto item = ReadUnder(context, id, unit)) g_socketRead.state = SocketState(*item);
	}
	return g_socketRead.state;
}

std::string CursorNotice(const D2RL::PluginContext* context, bool inventory, NativeUi::Focus focus, const NativeUi::Type& type,
	uint32_t focusedId, uintptr_t focusedUnit, const std::string& focusedName) {
	// Focus may pass through another control while the same item remains held.
	if (!inventory && !g_holdingItem.value_or(false)) return {};
	const auto held = InventoryCursor::Held(context);
	if (!held) return {};
	const bool wasHolding = g_holdingItem.value_or(false), wasFiller = g_heldFiller;
	const uint32_t heldBefore = std::exchange(g_heldId, held->id);
	g_holdingItem = held->id != 0;
	if (held->id != heldBefore) {
		g_heldFiller = held->id && NativeGroundItems::SocketFiller(held->classId);
		g_socketRead = {};
	}
	auto notice = inventory ? HandNotice(context, focus, type, *held, heldBefore, wasHolding, wasFiller, focusedId, focusedUnit) : std::string{};
	// Recorded after the notice, which judges the hand against what lay under it before.
	if (held->id) Hover(context, focusedId, focusedUnit, focusedName);
	else g_hovered = {};
	return notice;
}
}

std::string ItemName(uint32_t id, uint32_t classId, uint32_t quality) {
	const auto label = NativeGroundItems::ClassLabel(classId, quality);
	auto name = g_itemNames.Recall(id, label.baseName, quality);
	return name.empty() ? TableName(label.baseName, quality) : name;
}

void OfferDetail(std::string_view owner, std::string detail) {
	if (owner != g_detailFor) {
		g_detailFor = owner;
		g_detail.clear();
	}
	if (!detail.empty()) g_detail = std::move(detail);
}

bool HandlesFocus(NativeUi::Focus focus) {
	const auto type = NativeUi::Classify(focus.widget);
	const auto kind = type.kind;
	return IsInventory(kind) || kind == UiText::Kind::Button || kind == UiText::Kind::StatButton
		|| kind == UiText::Kind::TabButton || kind == UiText::Kind::ListItem || CharacterStats::HandlesFocus(focus, type)
		|| CharacterStatsReader::OwnsFocus(focus);
}

bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus, bool conversation, std::string_view tabChange) {
	if (conversation) { Reset(); return false; }
	// The character panel's own stick reader goes first: it claims all four
	// directions, and ReadDetail below would otherwise take up from it. Nothing
	// is lost by that order -- the character panel holds neither items nor
	// skills, so there is never a detail for up to read there.
	if (CharacterStatsReader::Tick(context, focus)) return true;
	NativeTooltips::Track(NativeWidgets::FindPanel("TooltipsPanel"));
	const auto type = NativeUi::Classify(focus.widget);
	const bool inventory = IsInventory(type.kind);
	const bool list = type.kind == UiText::Kind::ListItem;
	if (!list && !g_rowDetailFor.empty() && g_detailFor == g_rowDetailFor) {
		g_detail.clear();
		g_detailFor.clear();
	}
	const auto selection = NativeUi::FocusSelection(focus.widget);
	const bool button = type.kind == UiText::Kind::Button || type.kind == UiText::Kind::StatButton || type.kind == UiText::Kind::TabButton;
	const bool action = button || list;
	// The character panel's non-button rows: a label the controller stops on
	// through its focusable Tooltip child, with its value in a sibling widget.
	const bool stats = !inventory && !button && CharacterStats::HandlesFocus(focus, type);
	const auto progression = button ? NativeProgression::Read(focus, type) : NativeProgression::Details{};
	if (type.kind != UiText::Kind::StatButton) g_stat = {};
	UiText::Readout readout = inventory ? Inventory(context, focus, type)
		: stats ? CharacterStats::Read(focus, type) : UiText::Readout{};
	// Ordinary items can be named immediately; generated names await the tooltip
	// so the base name is never spoken as a preliminary, redundant announcement.
	const auto belt = type.name == "BeltWidget" ? BeltReader::Read(context, focus.widget) : BeltReader::Readout{};
	const auto itemSlot = type.name == "BeltWidget" ? belt.item
		: inventory ? NativeUi::FocusedItem(focus.widget) : NativeUi::ItemSlot{};
	Append(readout.option, belt.location);
	const auto presence = NativeUi::Presence(itemSlot);
	const auto equipmentPresence = type.kind == UiText::Kind::InventorySlot ? presence : NativeUi::ItemPresence::Unknown;
	const auto item = itemSlot.item ? NativeGroundItems::ItemUnitLabel(itemSlot.item) : NativeGroundItems::UnitLabel{};
	const bool generatedName = item.quality >= static_cast<uint32_t>(D2RL::Items::Quality::Magic);
	const auto& qualities = Strings::Current().itemQualities;
	const std::string quality = item.quality < qualities.size() ? qualities[item.quality] : "";
	std::string itemName;
	if (!generatedName && !item.baseName.empty()) {
		itemName = item.baseName;
		Append(itemName, quality);
	}
	// An item in hand hides the game's tooltips: a generated name is the one
	// the tooltip last gave, else the table name.
	else if (generatedName && g_holdingItem.value_or(false) && !item.baseName.empty()) {
		itemName = g_itemNames.Recall(item.id, item.baseName, item.quality);
		if (itemName.empty()) itemName = TableName(item.baseName, item.quality);
	}
	// An empty grid cell is told by its coordinates; a slot has none to say.
	else if (itemSlot.known && !itemSlot.item && type.kind != UiText::Kind::InventoryGrid) itemName = Strings::Current().slotEmpty;
	// With a gem, rune or jewel in hand, each item under it says whether it has room for one.
	const std::string sockets = inventory && g_holdingItem.value_or(false) && g_heldFiller && item.id
		? SocketsUnder(context, item.id, itemSlot.item) : std::string{};
	if (!inventory) g_namedFocus.clear();
	else if (readout.focusKey != g_namedFocus) {
		g_namedFocus = readout.focusKey;
		AsyncLog::InfoF(context, "[item] cell %s slot=%d known=%d unit=%p -> \"%s\"", readout.focusKey.c_str(),
			itemSlot.slot, itemSlot.known ? 1 : 0, reinterpret_cast<void*>(itemSlot.item), itemName.c_str());
	}
	// Moving to another cell drops the previous item's detail even before its
	// own tooltip renders, so a push up can never answer about the last item.
	if (inventory) OfferDetail(readout.focusKey, {});
	else if (progression.skill) OfferDetail(std::to_string(focus.widget), {});
	bool goldAmount = false; // the gold row is saying the amount in place of its label
	if (action) {
		const auto snapshot = NativeUi::Snapshot(focus.widget, focus.widget);
		readout = UiText::Describe(snapshot);
		readout.context = NativeWidgets::ReadName(focus.panel);
		readout.focusKey = std::to_string(focus.widget);
		if (list) {
			readout.focusKey += ":" + std::to_string(selection);
			OfferDetail(readout.focusKey, {});
		}
		if (!progression.readout.Empty() || progression.skill) {
			readout = progression.readout;
			if (!progression.skill && !snapshot.nodes.empty() && !snapshot.nodes.front().enabled)
				Append(readout.option, Strings::Current().unavailable);
		}
		// The inventory's gold button carries no text of its own -- MEASURED
		// 2026-09-20: [field] focus name="gold_button" ... text="" -- so landing
		// on it said only what the A button would do. The amount is a player
		// stat, read the same way a gold pile on the ground already is. It
		// REPLACES the label rather than joining it: "Drop Gold" is already the
		// A hint below, and a hint never leads the thing it acts on.
		const std::string focusName = NativeWidgets::ReadName(focus.widget);
		if (focusName == GoldButton) {
			if (const int gold = NativeGroundItems::PlayerGold(); gold >= 0) {
				char phrase[64]{};
				std::snprintf(phrase, sizeof(phrase), Strings::Current().goldCarried,
					std::to_string(gold).c_str());
				readout.option = phrase;
				goldAmount = true;
			}
		}
		// Its gold box has no focusable part on the controller layouts, so the
		// withdraw button says the amount it acts on, the same way.
		else if (focusName == StashGoldButton) {
			std::string gold = StashGold(context, focus.panel);
			if (const int stat = NativeGroundItems::PlayerStashGold(); gold.empty() && stat >= 0) gold = std::to_string(stat);
			if (!gold.empty()) {
				char phrase[96]{};
				std::snprintf(phrase, sizeof(phrase), Strings::Current().goldStashed, gold.c_str());
				readout.option = phrase;
				goldAmount = true;
			}
		}
	}
	const auto tooltip = NativeTooltips::Latest(now);
	bool renderedDescription = false, nameInOption = false;
	// What the tooltip adds to a name already spoken, kept so only that part is
	// announced when it arrives behind the name.
	std::string tooltipAddition, comparison, rowDetail;
	std::vector<std::string> rowTooltips;
	// Discard the previous focus/cell's last rendered description while the new
	// one is being built. Empty cells still expose the actual grid coordinates.
	if (tooltip.panel == focus.panel && tooltip.widget == focus.widget
		&& (!(inventory || list) || tooltip.selection == selection)
		&& equipmentPresence != NativeUi::ItemPresence::Empty) {
		std::string description;
		if (progression.skill) {
			description = ProgressionText::SkillDescription(tooltip.body);
			Append(description, UiText::Clean(tooltip.footer));
		} else if (inventory) {
			// The tooltip supplies the generated name and identification status;
			// prices and properties remain available through the detail gesture.
			const auto body = ItemBody(tooltip.body);
			const auto unidentified = UiText::Clean(Localization::Text(TooltipText::UnidentifiedKey));
			auto split = TooltipText::Split(body, StatLabels(), PriceLabels(), unidentified, RequirementLabels());
			if (!split.headline.empty() && !split.unidentified) {
				if (!generatedName && !itemName.empty()) split.headline = itemName;
				else Append(split.headline, quality);
				if (generatedName) g_itemNames.Remember(item.id, item.baseName, item.quality, split.headline);
			}
			// What a sighted player sees in red, said with the name.
			g_itemUnmet = split.unmet;
			if (!split.unmet.empty()) Append(split.headline, Refusal(split.unmet));
			Append(split.detail, UiText::Clean(tooltip.footer));
			comparison = ComparisonText(tooltip, split);
			// The engine stores a body bottom-up, so Read gives the lines back
			// in screen order and the name leads. The log still names the line
			// the headline came from, and whether the renderer styled it.
			if (auto seen = readout.focusKey + "|" + split.headline; seen != g_loggedTooltip) {
				g_loggedTooltip = std::move(seen);
				D2RAccess::AsyncLog::InfoF(context, "[item] %zu lines, title line %s -> \"%s\" | %s",
					body.lines.size(), body.Marked() ? std::to_string(body.heading).c_str() : "unstyled",
					split.headline.c_str(), split.detail.c_str());
				// How the game styled each line (colour of unmet requirements): docs/native-tooltips.md.
				if (const auto markup = TooltipText::Markup(tooltip.body); !markup.empty())
					D2RAccess::AsyncLog::InfoF(context, "[item-markup] %s", markup.c_str());
			}
			description = std::move(split.headline);
			if (AutoDetail::Enabled()) Append(description, split.detail);
			// An empty detail replaces the old one too, including when comparison
			// closes on an item with no further properties.
			g_detail = comparison.empty() ? std::move(split.detail) : comparison;
		} else if (list) {
			rowTooltips = RowTooltips(tooltip);
			for (const auto& text : rowTooltips) Append(rowDetail, text);
			description = rowDetail;
			g_detail = rowDetail;
		} else {
			description = UiText::Clean(tooltip.body);
			Append(description, UiText::Clean(tooltip.footer));
		}
		renderedDescription = !description.empty();
		if (progression.skill) {
			auto split = ProgressionText::SkillReadout(description, progression.points, progression.help);
			readout.option = std::move(split.headline);
			if (AutoDetail::Enabled()) Append(readout.option, split.detail);
			OfferDetail(std::to_string(focus.widget), std::move(split.detail));
		}
		// Not onto the gold amount: that row's tooltip is "Drop Gold", the A hint
		// again, and it renders ~400 ms after the row was read, so joining it
		// re-read the whole row (MEASURED 2026-09-21: "34 gold. Drop Gold").
		else if ((inventory || button) && !goldAmount && (type.kind != UiText::Kind::StatButton || progression.readout.Empty())) {
			// The unit named the item already; the tooltip only adds to that
			// name, or replaces one the item table cannot know.
			auto merge = MergeTooltip(itemName, std::move(description));
			if (!merge.corrects) Append(readout.option, itemName);
			Append(readout.option, merge.addition);
			tooltipAddition = std::move(merge.addition);
			nameInOption = true;
		}
		else if (!focus.panel) { readout.context = "World target"; readout.body = description; }
	}
	if (list && rowDetail.empty()) g_detail.clear();
	// Disappearing records must not interrupt a full description with its remaining fragment.
	const bool rowDetailChanged = !rowDetail.empty()
		&& (readout.focusKey != g_rowDetailFor || std::any_of(rowTooltips.begin(), rowTooltips.end(), [](const auto& text) {
			return std::find(g_rowTooltips.begin(), g_rowTooltips.end(), text) == g_rowTooltips.end();
		}));
	g_rowDetail = std::move(rowDetail);
	g_rowDetailFor = g_rowDetail.empty() ? std::string{} : readout.focusKey;
	g_rowTooltips = std::move(rowTooltips);
	const bool comparisonChanged = !comparison.empty()
		&& (comparison != g_comparison || readout.focusKey != g_comparisonFor);
	if (comparison.empty() && !g_comparison.empty() && !renderedDescription
		&& g_detailFor == g_comparisonFor) g_detail.clear();
	g_comparison = std::move(comparison);
	g_comparisonFor = g_comparison.empty() ? std::string{} : readout.focusKey;
	// Refresh the detail before consuming the gesture so comparison release or
	// a focus change cannot repeat a description belonging to the previous state.
	if (ReadDetail(context, focus)) return true;
	if (!nameInOption) Append(readout.option, itemName);
	Append(readout.option, sockets);
	Append(readout.option, belt.summary);
	// A named cell says what is in it, not where it is.
	if (type.kind == UiText::Kind::InventoryGrid && !readout.option.empty()) readout.coordinates.clear();
	if (type.kind == UiText::Kind::InventorySlot && !renderedDescription && itemName.empty()) {
		// The unit could not be named: say what is known of the slot and let the
		// tooltip refine it.
		if (equipmentPresence == NativeUi::ItemPresence::Occupied) Append(readout.option, Strings::Current().slotOccupied);
		// Neither a known slot nor readable contents: say what the focus is on
		// rather than let the move pass in silence.
		else if (readout.option.empty()) Append(readout.option, Strings::Current().equipment);
	}
	auto cursorNotice = CursorNotice(context, inventory, focus, type, item.id, itemSlot.item, itemName);
	const auto help = (inventory || action || stats) && !progression.skill ? NativeControllerHelp::ForFocus(focus) : std::string{};
	const auto helpContext = readout.context + ":" + readout.body;
	// Button hints belong after the thing they act on. A cell or slot holding an
	// item the unit could not name is still waiting for the game's own tooltip
	// (~350 ms), so the hints wait with it instead of arriving first in an
	// utterance of their own.
	const bool occupied = presence == NativeUi::ItemPresence::Occupied;
	const bool choosingSlot = type.kind == UiText::Kind::InventorySlot && g_holdingItem.value_or(false);
	if (inventory) {
		const bool pressed = choosingSlot
			&& InputDirection::LastFaceButton(EquipAttempt::PressWindowMs) == InputDirection::FaceButton::A;
		const uint32_t held = g_holdingItem.value_or(false) ? InventoryCursor::HeldItemId(context).value_or(0) : 0;
		if (g_equipAttempt.Update(pressed, held, now)) {
			// A gem on a worn item's slot was a socketing the game refused, not an equip.
			const auto refusal = g_heldFiller && equipmentPresence == NativeUi::ItemPresence::Occupied
				? std::string(Strings::Current().socketRefused) : Refusal(g_heldUnmet);
			D2RAccess::AsyncLog::InfoF(context, "[equip] refused -> \"%s\"", refusal.c_str());
			ScreenReader::Speak(refusal, true);
		}
	}
	const bool awaitingDescription = inventory && !choosingSlot && !renderedDescription && occupied && itemName.empty();
	const bool helpChanged = help != g_spokenHelp || helpContext != g_helpContext;
	g_help = help;
	if (!focus.panel && readout.Empty()) {
		const auto target = NativeUi::Snapshot(NativeWidgets::FindPanel(MonsterPlate), 0);
		if (target.surface == UiText::Surface::Target) readout = UiText::Describe(target);
	}
	if (!focus.panel && GroundItemService::OwnsWorldLabel(readout.body)) {
		readout = {};
		renderedDescription = false;
	}
	// The game targets through walls, so either world label — the monster health
	// plate and the world target tooltip — can name something in the next room,
	// which the right-stick scan would never list. Those are dropped and the
	// reader is left with nothing to say, so the same entity announces itself as
	// soon as it is really in sight.
	const auto bearer = !focus.panel && !readout.Empty() ? EntityRadarService::LabelTarget(readout.body) : TargetSight::Bearer{};
	// A monster the plate names is said once it is within melee reach (a ranged
	// attack reaches what the auto-target picks), and not again while the
	// auto-target keeps trading it with the rest of its pack.
	const bool monsterLabel = !focus.panel && !readout.Empty() && bearer.sight != TargetSight::Sight::Hidden
		&& (bearer.sight == TargetSight::Sight::Visible ? bearer.kind == EntityRadar::Kind::Enemy : readout.context == MonsterPlate);
	if (bearer.sight == TargetSight::Sight::Hidden) {
		if (g_hiddenLabel != readout.body) {
			g_hiddenLabel = readout.body;
			AsyncLog::InfoF(context, "[interaction] behind a wall, not spoken -> \"%s\"", readout.body.c_str());
		}
		readout = {};
		renderedDescription = false;
	} else if (!readout.Empty()) g_hiddenLabel.clear();
	if (!monsterLabel) g_monsterNames.Absent();
	else {
		const bool visible = bearer.sight == TargetSight::Sight::Visible;
		if (!g_monsterNames.Admit(readout.body, visible ? std::optional{bearer.distance} : std::nullopt,
			visible && bearer.important, EntityCueService::NamingReach(), now)) {
			if (g_quietMonster != readout.body) {
				g_quietMonster = readout.body;
				AsyncLog::InfoF(context, "[interaction] out of reach or just heard, not spoken -> \"%s\" distance=%.1f",
					readout.body.c_str(), visible ? bearer.distance : -1.0);
			}
			readout = {};
			renderedDescription = false;
		} else {
			g_quietMonster.clear();
			readout.body = MonsterNames::Speakable(readout.body);
		}
	}
	if (inventory != g_inventory) {
		if (g_inventory) ScreenReader::Stop();
		g_reader.Reset();
	}
	g_inventory = inventory;
	if (!inventory) g_inventoryContext = {};
	if (!tabChange.empty()) {
		// A tab switch announces only its name. Accept the current cell as the
		// baseline so its delayed readout cannot immediately interrupt that name.
		if (readout.Empty()) g_reader.Reset();
		else (void)g_reader.Update(readout, now, 0);
		// The menu switcher's tab is the panel the window announcer has just
		// named. MEASURED 2026-09-21: "Character" was said twice, the second
		// time with interrupt, which cut off what was queued behind the first.
		const bool named = PanelState::TakeAnnounced(tabChange);
		if (named) D2RAccess::AsyncLog::InfoF(context, "[tabs] %s already named by the window, not repeated", std::string(tabChange).c_str());
		std::string text(named ? std::string_view{} : tabChange);
		Append(text, cursorNotice);
		if (!text.empty()) ScreenReader::Speak(text, !named);
		return true;
	}
	// The stock item tooltip can appear ~350 ms after selection. A grid cell
	// whose item the unit could not name waits for it without speaking
	// coordinates first; other cells keep their quick coordinate feedback.
	if ((type.kind == UiText::Kind::InventoryGrid || generatedName) && awaitingDescription) {
		if (!cursorNotice.empty()) {
			D2RAccess::AsyncLog::InfoF(context, "[interaction] %s placement -> \"%s\"", readout.context.c_str(), cursorNotice.c_str());
			ScreenReader::Speak(cursorNotice, true);
		}
		return true; // the hints stay pending until the description arrives
	}
	if (progression.skill && !renderedDescription) return true;
	if (readout.Empty() && help.empty()) { g_reader.Reset(); return inventory || action || stats; }
	// A name read from the unit, a rendered focus-matching description and an
	// equipment slot are complete already. Cells without one wait a single poll
	// for capture, rather than the dialogue delay.
	const bool complete = renderedDescription || !itemName.empty()
		|| type.kind == UiText::Kind::InventorySlot || !cursorNotice.empty();
	// A monster name the gate admits is new by construction; waiting for it to
	// settle loses it to the auto-target's next hop (live log, 2026-10-02).
	auto announcement = action || monsterLabel ? g_reader.Update(readout, now, 0)
		: inventory ? g_reader.Update(readout, now, complete ? 0 : InventorySettleMs)
		: g_reader.Update(readout, now);
	if (type.kind == UiText::Kind::StatButton && !progression.readout.Empty()) {
		std::string changed;
		if (g_stat.widget == focus.widget) {
			if (g_stat.value != progression.value) Append(changed, progression.value);
			if (g_stat.points != progression.points) Append(changed, progression.points);
		}
		if (!changed.empty() && !announcement.text.empty()) announcement.text = std::move(changed);
		g_stat = {focus.widget, progression.value, progression.points};
	}
	if (comparisonChanged) announcement.text = g_comparison;
	if (rowDetailChanged) Append(announcement.text, g_rowDetail);
	// A control hint follows the thing it acts on and never leads. While the
	// focus has not been described yet the hint waits for the readout that
	// will describe it -- that is how "B: Close Menu" arrived before "Belt.
	// Empty". Once the player has heard where they are, a hint that changes
	// under them still speaks on its own: the merchant's action list changing
	// is news, and the item it applies to was named a moment ago.
	const bool describedNow = !announcement.text.empty();
	const bool describedHere = readout.focusKey.empty() || readout.focusKey == g_describedFocus;
	const bool helpFollows = helpChanged && !awaitingDescription && (describedNow || describedHere);
	// A hint arriving on its own is the tail of the readout just spoken, not a
	// new context: the legend repopulates ~60 ms after the focus moves (MEASURED
	// 2026-09-21, character panel), so it queues instead of cutting the row off.
	bool interrupt = !(helpFollows && !describedNow);
	if (monsterLabel) interrupt = false; // incremental combat news, never a context change
	// The tooltip arriving 400 ms behind a name already spoken for this focus is
	// the tail of that utterance, or a correction to it. Either way only what it
	// adds is said, and it queues instead of cutting the name off mid-word.
	if (!comparisonChanged && !itemName.empty() && renderedDescription && describedHere) {
		interrupt = false;
		if (describedNow && !tooltipAddition.empty()) announcement.text = tooltipAddition;
	}
	if (helpFollows) Append(announcement.text, help);
	if (describedNow) g_describedFocus = readout.focusKey;
	if (!cursorNotice.empty()) {
		Append(cursorNotice, announcement.text);
		announcement.text = std::move(cursorNotice);
		interrupt = true; // picking an item up or putting it down is news
	}
	// Only what was actually spoken counts as spoken. An empty hint is
	// recorded either way, so leaving a panel does not leave the last hint
	// looking unchanged when the player comes back to it.
	if (!awaitingDescription && (helpFollows || help.empty())) {
		g_spokenHelp = help;
		g_helpContext = helpContext;
	}
	if (!announcement.text.empty()) {
		D2RAccess::AsyncLog::InfoF(context, "[interaction] %s -> \"%s\"", readout.context.c_str(), announcement.text.c_str());
		ScreenReader::Speak(announcement.text, interrupt);
	}
	return inventory || action || stats;
}

void Reset() {
	g_reader.Reset();
	g_inventory = false;
	g_inventoryContext = {};
	g_stat = {};
	g_help.clear();
	g_spokenHelp.clear();
	g_describedFocus.clear();
	g_helpContext.clear();
	g_hiddenLabel.clear();
	g_quietMonster.clear();
	g_monsterNames.Reset();
	g_namedFocus.clear();
	g_loggedTooltip.clear();
	g_holdingItem.reset();
	g_heldId = 0;
	g_heldFiller = false;
	g_hovered = {};
	g_socketRead = {};
	g_itemUnmet.clear();
	g_heldUnmet.clear();
	g_equipAttempt.Reset();
	g_slotLocations.clear();
	g_detail.clear();
	g_detailFor.clear();
	g_comparison.clear();
	g_comparisonFor.clear();
	g_rowDetail.clear();
	g_rowDetailFor.clear();
	g_rowTooltips.clear();
	CharacterStats::Reset();
	CharacterStatsReader::Reset();
	NativeControllerHelp::Reset();
	NativeTooltips::Reset();
}

std::string Repeat() {
	std::string text = g_inventory && g_holdingItem.value_or(false) ? Strings::Current().itemInHand : "";
	Append(text, g_comparison.empty() ? g_reader.Repeat() : g_comparison);
	Append(text, g_rowDetail);
	Append(text, g_help);
	return text;
}
}
