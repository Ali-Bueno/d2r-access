#include "async_log.h"
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
std::optional<bool> g_holdingItem;

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

UiText::Readout Inventory(NativeUi::Focus focus, const NativeUi::Type& type) {
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
		result.option = phrase;
	}
	if (type.kind == UiText::Kind::InventorySlot) {
		const int location = NativeUi::EquipmentLocation(focus.widget);
		const auto& labels = Strings::Current().equipmentSlots;
		if (location > 0 && static_cast<size_t>(location) < labels.size()) Append(result.option, labels[location]);
	}
	return result;
}

std::string CursorNotice(const D2RL::PluginContext* context, bool inventory) {
	if (!inventory) { g_holdingItem.reset(); return {}; }
	const auto holding = InventoryCursor::HoldingItem(context);
	if (!holding || holding == g_holdingItem) return {};
	const bool wasHolding = g_holdingItem.value_or(false);
	g_holdingItem = holding;
	if (*holding) return Strings::Current().itemInHand;
	return wasHolding ? Strings::Current().itemReleased : "";
}
}

bool HandlesFocus(NativeUi::Focus focus) {
	const auto kind = NativeUi::Classify(focus.widget).kind;
	return IsInventory(kind) || kind == UiText::Kind::Button || kind == UiText::Kind::StatButton || kind == UiText::Kind::TabButton;
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
	const auto progression = button ? NativeProgression::Read(focus, type) : NativeProgression::Details{};
	if (type.kind != UiText::Kind::StatButton) g_stat = {};
	UiText::Readout readout = inventory ? Inventory(focus, type) : UiText::Readout{};
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
		if (type.kind == UiText::Kind::InventoryGrid && renderedDescription) readout.option = description;
		else if (progression.skill) {
			readout.option = ProgressionText::SkillReadout(description,
				readout.option, progression.points, progression.help);
		}
		else if ((inventory || button) && (type.kind != UiText::Kind::StatButton || progression.readout.Empty())) Append(readout.option, description);
		else if (!focus.panel) { readout.context = "World target"; readout.body = description; }
	}
	if (type.kind == UiText::Kind::InventorySlot && !renderedDescription) {
		if (equipmentPresence == NativeUi::ItemPresence::Empty) Append(readout.option, Strings::Current().slotEmpty);
		else if (equipmentPresence == NativeUi::ItemPresence::Occupied) Append(readout.option, Strings::Current().slotOccupied);
	}
	const auto help = (inventory || button) && !progression.skill ? NativeControllerHelp::ForFocus(focus) : std::string{};
	const auto helpContext = readout.context + ":" + readout.body;
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
	if (inventory != g_inventory) {
		if (g_inventory) ScreenReader::Stop();
		g_reader.Reset();
	}
	g_inventory = inventory;
	if (!inventory) g_inventoryContext = {};
	auto cursorNotice = CursorNotice(context, inventory);
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
	if (type.kind == UiText::Kind::InventoryGrid && !renderedDescription
		&& NativeUi::GridItemPresence(focus.widget) == NativeUi::ItemPresence::Occupied) {
		if (helpChanged) Append(cursorNotice, help);
		if (!cursorNotice.empty()) {
			D2RAccess::AsyncLog::InfoF(context, "[interaction] %s actions -> \"%s\"", readout.context.c_str(), cursorNotice.c_str());
			ScreenReader::Speak(cursorNotice, true);
		}
		g_spokenHelp = help;
		g_helpContext = helpContext;
		return true;
	}
	if (progression.skill && !renderedDescription) return true;
	if (readout.Empty() && help.empty()) { g_reader.Reset(); return inventory || button; }
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
	if (helpChanged) Append(announcement.text, help);
	if (!cursorNotice.empty()) {
		Append(cursorNotice, announcement.text);
		announcement.text = std::move(cursorNotice);
	}
	g_spokenHelp = help;
	g_helpContext = helpContext;
	if (!announcement.text.empty()) {
		D2RAccess::AsyncLog::InfoF(context, "[interaction] %s -> \"%s\"", readout.context.c_str(), announcement.text.c_str());
		ScreenReader::Speak(announcement.text, true);
	}
	return inventory || button;
}

void Reset() {
	g_reader.Reset();
	g_inventory = false;
	g_inventoryContext = {};
	g_stat = {};
	g_help.clear();
	g_spokenHelp.clear();
	g_helpContext.clear();
	g_holdingItem.reset();
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
