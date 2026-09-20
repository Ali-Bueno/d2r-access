#include "async_log.h"
#include "character_stats.h"
#include "entity_radar_service.h"
#include "interaction_reader.h"
#include "inventory_cursor.h"
#include "ground_item_service.h"
#include "localization.h"
#include "native_tooltips.h"
#include "native_controller_help.h"
#include "native_progression.h"
#include "progression_text.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "tab_reader.h"
#include "ui_text.h"

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

bool IsInventory(UiText::Kind kind) {
	return kind == UiText::Kind::InventoryGrid || kind == UiText::Kind::InventorySlot || kind == UiText::Kind::InventoryItem;
}

std::string Title(uintptr_t panel) {
	const bool stash = NativeUi::Classify(panel).name == "BankPanel";
	const auto localized = Localization::Text(stash ? "stash" : "minipanelinv");
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

bool HandlesFocus(NativeUi::Focus focus) {
	const auto type = NativeUi::Classify(focus.widget);
	const auto kind = type.kind;
	return IsInventory(kind) || kind == UiText::Kind::Button || kind == UiText::Kind::StatButton
		|| kind == UiText::Kind::TabButton || CharacterStats::HandlesFocus(focus, type);
}

bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus, bool conversation, std::string_view tabChange) {
	if (conversation) { Reset(); return false; }
	NativeTooltips::Track(NativeWidgets::FindPanel("TooltipsPanel"));
	const auto type = NativeUi::Classify(focus.widget);
	const bool inventory = IsInventory(type.kind);
	const auto selection = NativeUi::FocusSelection(focus.widget);
	const auto equipmentPresence = type.kind == UiText::Kind::InventorySlot
		? NativeUi::EquipmentItemPresence(focus.widget) : NativeUi::ItemPresence::Unknown;
	const bool button = type.kind == UiText::Kind::Button || type.kind == UiText::Kind::StatButton || type.kind == UiText::Kind::TabButton;
	// The character panel's non-button rows: a label the controller stops on
	// through its focusable Tooltip child, with its value in a sibling widget.
	const bool stats = !inventory && !button && CharacterStats::HandlesFocus(focus, type);
	const auto progression = button ? NativeProgression::Read(focus, type) : NativeProgression::Details{};
	if (type.kind != UiText::Kind::StatButton) g_stat = {};
	UiText::Readout readout = inventory ? Inventory(context, focus, type)
		: stats ? CharacterStats::Read(focus, type) : UiText::Readout{};
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
	}
	const auto tooltip = NativeTooltips::Latest(now);
	bool renderedDescription = false;
	// Discard the previous focus/cell's last rendered description while the new
	// one is being built. Empty cells still expose the actual grid coordinates.
	if (tooltip.panel == focus.panel && tooltip.widget == focus.widget
		&& (!inventory || tooltip.selection == selection)
		&& equipmentPresence != NativeUi::ItemPresence::Empty) {
		std::string description = progression.skill ? ProgressionText::SkillDescription(tooltip.body) : UiText::Clean(tooltip.body);
		Append(description, UiText::Clean(tooltip.footer));
		renderedDescription = !description.empty();
		if (type.kind == UiText::Kind::InventoryGrid && renderedDescription) {
			readout.option = description;
			readout.coordinates.clear(); // a described cell keeps its existing coordinate-free readout
		}
		else if (progression.skill) {
			readout.option = ProgressionText::SkillReadout(description, progression.points, progression.help);
		}
		else if ((inventory || button) && (type.kind != UiText::Kind::StatButton || progression.readout.Empty())) Append(readout.option, description);
		else if (!focus.panel) { readout.context = "World target"; readout.body = description; }
	}
	if (type.kind == UiText::Kind::InventorySlot && !renderedDescription) {
		if (equipmentPresence == NativeUi::ItemPresence::Empty) Append(readout.option, Strings::Current().slotEmpty);
		else if (equipmentPresence == NativeUi::ItemPresence::Occupied) Append(readout.option, Strings::Current().slotOccupied);
		// Neither a known slot nor readable contents: say what the focus is on
		// rather than let the move pass in silence.
		else if (readout.option.empty()) Append(readout.option, Strings::Current().equipment);
	}
	const auto help = (inventory || button || stats) && !progression.skill ? NativeControllerHelp::ForFocus(focus) : std::string{};
	const auto helpContext = readout.context + ":" + readout.body;
	// Button hints belong after the thing they act on. An occupied cell or slot
	// is still waiting for the game's own tooltip (~350 ms), so the hints wait
	// with it instead of arriving first in an utterance of their own.
	const bool occupied = type.kind == UiText::Kind::InventorySlot
		? equipmentPresence == NativeUi::ItemPresence::Occupied
		: type.kind == UiText::Kind::InventoryGrid
			&& NativeUi::GridItemPresence(focus.widget) == NativeUi::ItemPresence::Occupied;
	const bool awaitingDescription = inventory && !renderedDescription && occupied;
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
		std::string text(tabChange);
		Append(text, cursorNotice);
		ScreenReader::Speak(text, true);
		return true;
	}
	// The stock item tooltip can appear ~350 ms after selection. An occupied
	// grid cell waits for it without speaking coordinates first; other cells
	// keep their existing quick coordinate feedback.
	if (type.kind == UiText::Kind::InventoryGrid && occupied && !renderedDescription) {
		if (!cursorNotice.empty()) {
			D2RAccess::AsyncLog::InfoF(context, "[interaction] %s placement -> \"%s\"", readout.context.c_str(), cursorNotice.c_str());
			ScreenReader::Speak(cursorNotice, true);
		}
		return true; // the hints stay pending until the description arrives
	}
	if (progression.skill && !renderedDescription) return true;
	if (readout.Empty() && help.empty()) { g_reader.Reset(); return inventory || button || stats; }
	// A rendered, focus-matching item description is already complete. Cells
	// without one wait a single poll for capture, rather than the dialogue delay.
	auto announcement = button ? g_reader.Update(readout, now, 0)
		: inventory ? g_reader.Update(readout, now, renderedDescription || type.kind == UiText::Kind::InventorySlot || !cursorNotice.empty() ? 0 : InventorySettleMs)
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
	if (helpChanged && !awaitingDescription) Append(announcement.text, help);
	if (!cursorNotice.empty()) {
		Append(cursorNotice, announcement.text);
		announcement.text = std::move(cursorNotice);
	}
	if (!awaitingDescription) {
		g_spokenHelp = help;
		g_helpContext = helpContext;
	}
	if (!announcement.text.empty()) {
		D2RAccess::AsyncLog::InfoF(context, "[interaction] %s -> \"%s\"", readout.context.c_str(), announcement.text.c_str());
		ScreenReader::Speak(announcement.text, true);
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
	g_helpContext.clear();
	g_hiddenLabel.clear();
	g_holdingItem.reset();
	g_slotLocations.clear();
	CharacterStats::Reset();
	NativeControllerHelp::Reset();
	NativeTooltips::Reset();
}

std::string Repeat() {
	std::string text = g_holdingItem.value_or(false) ? Strings::Current().itemInHand : "";
	Append(text, g_reader.Repeat());
	Append(text, g_help);
	return text;
}
}
