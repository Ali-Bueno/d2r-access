#include "interaction_reader.h"
#include "localization.h"
#include "native_tooltips.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "ui_text.h"

#include <cstdio>

namespace D2RAccess::InteractionReader {
namespace {
UiText::Reader g_reader;
bool g_inventory = false;

void Append(std::string& text, const std::string& part) {
	if (part.empty()) return;
	if (!text.empty()) text += ". ";
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
	UiText::Readout result;
	result.context = NativeWidgets::ReadName(focus.panel);
	result.body = Title(focus.panel);
	// Tabs are selected native values, never an enumeration of all storage pages.
	for (const auto& node : NativeUi::Snapshot(focus.panel, focus.widget).nodes) {
		if (node.kind == UiText::Kind::Tabs) Append(result.body, UiText::Clean(node.text));
	}
	if (type.kind == UiText::Kind::InventorySlot) Append(result.body, Strings::Current().equipment);
	else if (type.name == "BeltWidget") Append(result.body, Strings::Current().belt);
	result.focusKey = std::to_string(focus.widget) + ":" + std::to_string(NativeUi::FocusSelection(focus.widget));
	const auto cell = NativeUi::GridCell(focus.widget);
	if (cell.x >= 0) {
		char phrase[100]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().gridCell, cell.y + 1, cell.x + 1);
		result.option = phrase;
	}
	return result;
}
}

bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus, bool conversation) {
	if (conversation) { Reset(); return false; }
	NativeTooltips::Track(NativeWidgets::FindPanel("TooltipsPanel"));
	const auto type = NativeUi::Classify(focus.widget);
	const bool inventory = IsInventory(type.kind);
	UiText::Readout readout = inventory ? Inventory(focus, type) : UiText::Readout{};
	const auto tooltip = NativeTooltips::Latest(now);
	// Discard the previous focus/cell's last rendered description while the new
	// one is being built. Empty cells still expose the actual grid coordinates.
	if (tooltip.panel == focus.panel && tooltip.widget == focus.widget
		&& (!inventory || tooltip.selection == NativeUi::FocusSelection(focus.widget))) {
		std::string description = UiText::Clean(tooltip.body);
		Append(description, UiText::Clean(tooltip.footer));
		if (inventory) Append(readout.option, description);
		else if (!focus.panel) { readout.context = "World target"; readout.body = description; }
	}
	if (!focus.panel && readout.Empty()) {
		const auto target = NativeUi::Snapshot(NativeWidgets::FindPanel("HUDMonsterHealth"), 0);
		if (target.surface == UiText::Surface::Target) readout = UiText::Describe(target);
	}
	if (inventory != g_inventory) {
		if (g_inventory) ScreenReader::Stop();
		g_reader.Reset();
	}
	g_inventory = inventory;
	if (readout.Empty()) { g_reader.Reset(); return inventory; }
	const auto announcement = g_reader.Update(readout, now);
	if (!announcement.text.empty()) {
		D2RL::LogInfoF(context, "[interaction] %s -> \"%s\"", readout.context.c_str(), announcement.text.c_str());
		ScreenReader::Speak(announcement.text, true);
	}
	return inventory;
}

void Reset() {
	g_reader.Reset();
	g_inventory = false;
	NativeTooltips::Reset();
}

std::string Repeat() { return g_reader.Repeat(); }
}
