#include "async_log.h"
#include "auto_detail.h"
#include "character_stats.h"
#include "character_stats_reader.h"
#include "entity_radar_service.h"
#include "interaction_reader.h"
#include "inventory_cursor.h"
#include "ground_item_service.h"
#include "input_direction.h"
#include "localization.h"
#include "native_tooltips.h"
#include "native_controller_help.h"
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
#include <cstdio>

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
std::string g_hiddenLabel;
std::optional<bool> g_holdingItem;
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

// The cell whose item was named, and the cell and headline its tooltip was
// logged under, so [item] is written once per item instead of on every UI poll
// while a tooltip is on screen (each line is an open, write and flush --
// async_log.cpp).
std::string g_namedFocus, g_loggedTooltip;

// The game's own labels for a weapon's damage and an armour's defense, so the
// split can tell them from the item's name in any language. Each table entry is a printf format, so only the part before its
// first placeholder can begin a rendered line -- resolving "Defense: %d" and
// then looking for that whole string never matched anything. Resolved once per
// language: the string tables do not change under a running game.
const std::vector<std::string>& StatLabels() {
	static std::vector<std::string> cached = [] {
		std::vector<std::string> labels;
		for (const char* key : TooltipText::BaseStatKeys) {
			auto text = TooltipText::LabelPrefix(Localization::Text(key));
			if (!text.empty()) labels.push_back(std::move(text));
		}
		return labels;
	}();
	return cached;
}

// Price labels distinguish the item's name from a shop tooltip's first line.
const std::vector<std::string>& PriceLabels() {
	static std::vector<std::string> cached = [] {
		std::vector<std::string> labels;
		for (const char* key : {TooltipText::PriceKey, TooltipText::SellKey}) {
			auto text = TooltipText::LabelPrefix(Localization::Text(key));
			if (!text.empty()) labels.push_back(std::move(text));
		}
		return labels;
	}();
	return cached;
}

// A push up on the right stick, claimed only while a panel is open so the
// entity scan and the pathfinder keep the stick during play.
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
		const auto body = TooltipText::Read(record.body);
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

bool IsInventory(UiText::Kind kind) {
	return kind == UiText::Kind::InventoryGrid || kind == UiText::Kind::InventorySlot || kind == UiText::Kind::InventoryItem;
}

std::string Title(uintptr_t panel) {
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
	if (type.name == "BeltWidget") return Strings::Current().belt;
	const auto cell = NativeUi::GridCell(focus.widget);
	if (cell.x < 0) return {};
	char phrase[100]{};
	std::snprintf(phrase, sizeof(phrase), Strings::Current().gridCell, cell.y + 1, cell.x + 1);
	return phrase;
}

std::string CursorNotice(const D2RL::PluginContext* context, bool inventory, NativeUi::Focus focus, const NativeUi::Type& type) {
	if (!inventory) { g_holdingItem.reset(); return {}; }
	const auto holding = InventoryCursor::HoldingItem(context);
	if (!holding || holding == g_holdingItem) return {};
	const bool wasHolding = g_holdingItem.value_or(false);
	g_holdingItem = holding;
	if (*holding) return Strings::Current().itemInHand;
	if (!wasHolding) return {};
	const auto destination = Destination(context, focus, type);
	if (destination.empty()) return Strings::Current().itemReleased;
	char phrase[200]{};
	std::snprintf(phrase, sizeof(phrase), Strings::Current().itemPlacedIn, destination.c_str());
	return phrase;
}
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
		|| kind == UiText::Kind::TabButton || CharacterStats::HandlesFocus(focus, type);
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
	const auto selection = NativeUi::FocusSelection(focus.widget);
	const bool button = type.kind == UiText::Kind::Button || type.kind == UiText::Kind::StatButton || type.kind == UiText::Kind::TabButton;
	// The character panel's non-button rows: a label the controller stops on
	// through its focusable Tooltip child, with its value in a sibling widget.
	const bool stats = !inventory && !button && CharacterStats::HandlesFocus(focus, type);
	const auto progression = button ? NativeProgression::Read(focus, type) : NativeProgression::Details{};
	if (type.kind != UiText::Kind::StatButton) g_stat = {};
	UiText::Readout readout = inventory ? Inventory(context, focus, type)
		: stats ? CharacterStats::Read(focus, type) : UiText::Readout{};
	// Ordinary items can be named immediately; generated names await the tooltip
	// so the base name is never spoken as a preliminary, redundant announcement.
	const auto itemSlot = inventory ? NativeUi::FocusedItem(focus.widget) : NativeUi::ItemSlot{};
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
	// An empty grid cell is told by its coordinates; a slot has none to say.
	else if (itemSlot.known && !itemSlot.item && type.kind != UiText::Kind::InventoryGrid) itemName = Strings::Current().slotEmpty;
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
	if (button) {
		const auto snapshot = NativeUi::Snapshot(focus.widget, focus.widget);
		readout = UiText::Describe(snapshot);
		readout.context = NativeWidgets::ReadName(focus.panel);
		readout.focusKey = std::to_string(focus.widget);
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
		if (NativeWidgets::ReadName(focus.widget) == GoldButton) {
			if (const int gold = NativeGroundItems::PlayerGold(); gold >= 0) {
				char phrase[64]{};
				std::snprintf(phrase, sizeof(phrase), Strings::Current().goldCarried,
					std::to_string(gold).c_str());
				readout.option = phrase;
				goldAmount = true;
			}
		}
	}
	const auto tooltip = NativeTooltips::Latest(now);
	bool renderedDescription = false, nameInOption = false;
	// What the tooltip adds to a name already spoken, kept so only that part is
	// announced when it arrives behind the name.
	std::string tooltipAddition, comparison;
	// Discard the previous focus/cell's last rendered description while the new
	// one is being built. Empty cells still expose the actual grid coordinates.
	if (tooltip.panel == focus.panel && tooltip.widget == focus.widget
		&& (!inventory || tooltip.selection == selection)
		&& equipmentPresence != NativeUi::ItemPresence::Empty) {
		std::string description;
		if (progression.skill) {
			description = ProgressionText::SkillDescription(tooltip.body);
			Append(description, UiText::Clean(tooltip.footer));
		} else if (inventory) {
			// The tooltip supplies the generated name and identification status;
			// prices and properties remain available through the detail gesture.
			const auto body = TooltipText::Read(tooltip.body);
			const auto unidentified = UiText::Clean(Localization::Text(TooltipText::UnidentifiedKey));
			auto split = TooltipText::Split(body, StatLabels(), PriceLabels(), unidentified);
			if (!split.headline.empty() && !split.unidentified) {
				if (!generatedName && !itemName.empty()) split.headline = itemName;
				else Append(split.headline, quality);
			}
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
			}
			description = std::move(split.headline);
			if (AutoDetail::Enabled()) Append(description, split.detail);
			// An empty detail replaces the old one too, including when comparison
			// closes on an item with no further properties.
			g_detail = comparison.empty() ? std::move(split.detail) : comparison;
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
	const auto help = (inventory || button || stats) && !progression.skill ? NativeControllerHelp::ForFocus(focus) : std::string{};
	const auto helpContext = readout.context + ":" + readout.body;
	// Button hints belong after the thing they act on. A cell or slot holding an
	// item the unit could not name is still waiting for the game's own tooltip
	// (~350 ms), so the hints wait with it instead of arriving first in an
	// utterance of their own.
	const bool occupied = presence == NativeUi::ItemPresence::Occupied;
	const bool awaitingDescription = inventory && !renderedDescription && occupied && itemName.empty();
	const bool helpChanged = help != g_spokenHelp || helpContext != g_helpContext;
	g_help = help;
	if (!focus.panel && readout.Empty()) {
		const auto target = NativeUi::Snapshot(NativeWidgets::FindPanel("HUDMonsterHealth"), 0);
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
	if (!focus.panel && !readout.Empty()
		&& EntityRadarService::LabelSight(readout.body) == TargetSight::Sight::Hidden) {
		if (g_hiddenLabel != readout.body) {
			g_hiddenLabel = readout.body;
			AsyncLog::InfoF(context, "[interaction] behind a wall, not spoken -> \"%s\"", readout.body.c_str());
		}
		readout = {};
		renderedDescription = false;
	} else if (!readout.Empty()) g_hiddenLabel.clear();
	if (inventory != g_inventory) {
		if (g_inventory) ScreenReader::Stop();
		g_reader.Reset();
	}
	g_inventory = inventory;
	if (!inventory) g_inventoryContext = {};
	auto cursorNotice = CursorNotice(context, inventory, focus, type);
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
	if (readout.Empty() && help.empty()) { g_reader.Reset(); return inventory || button || stats; }
	// A name read from the unit, a rendered focus-matching description and an
	// equipment slot are complete already. Cells without one wait a single poll
	// for capture, rather than the dialogue delay.
	const bool complete = renderedDescription || !itemName.empty()
		|| type.kind == UiText::Kind::InventorySlot || !cursorNotice.empty();
	auto announcement = button ? g_reader.Update(readout, now, 0)
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
	return inventory || button || stats;
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
	g_namedFocus.clear();
	g_loggedTooltip.clear();
	g_holdingItem.reset();
	g_slotLocations.clear();
	g_detail.clear();
	g_detailFor.clear();
	g_comparison.clear();
	g_comparisonFor.clear();
	CharacterStats::Reset();
	CharacterStatsReader::Reset();
	NativeControllerHelp::Reset();
	NativeTooltips::Reset();
}

std::string Repeat() {
	std::string text = g_holdingItem.value_or(false) ? Strings::Current().itemInHand : "";
	Append(text, g_comparison.empty() ? g_reader.Repeat() : g_comparison);
	Append(text, g_help);
	return text;
}
}
