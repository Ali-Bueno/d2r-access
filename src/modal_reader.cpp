#include "modal_reader.h"

#include "async_log.h"
#include "character_stats_reader.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "ui_text.h"

#include <algorithm>
#include <string>

namespace D2RAccess::ModalReader {

namespace {

std::string g_lastPanel;

bool Speakable(const UiText::Node& node) {
	if (node.text.empty() || !node.enabled) return false;
	switch (node.kind) {
	case UiText::Kind::Text:
	case UiText::Kind::LinkedText:
	case UiText::Kind::ScrollingText:
	case UiText::Kind::Legend:
	case UiText::Kind::Button:
	case UiText::Kind::Focusable:
		return true;
	default:
		return false;
	}
}

bool HasInventory(const UiText::Snapshot& snapshot) {
	return std::any_of(snapshot.nodes.begin(), snapshot.nodes.end(), [](const auto& node) {
		return node.kind == UiText::Kind::InventoryGrid || node.kind == UiText::Kind::InventorySlot
			|| node.kind == UiText::Kind::InventoryItem;
	});
}

// A tab bar's captions belong to the tab reader, which names the selected one
// as the player moves along the bar. Read here they are every caption at once
// with no selection -- the quest log's act tabs came out as "I, II, III, IV,
// V". They are the bar's own Text<N> children, the same boxes
// NativeUi::SelectedTabText reads.
bool UnderTabBar(const UiText::Snapshot& snapshot, int parent) {
	for (int at = parent; at >= 0 && static_cast<size_t>(at) < snapshot.nodes.size();
	     at = snapshot.nodes[static_cast<size_t>(at)].parent) {
		if (snapshot.nodes[static_cast<size_t>(at)].kind == UiText::Kind::Tabs) return true;
	}
	return false;
}

void Append(std::string& phrase, const std::string& part) {
	if (part.empty()) return;
	if (!phrase.empty()) phrase += ", ";
	phrase += part;
}

}

void Reset() { g_lastPanel.clear(); }

void Tick(const D2RL::PluginContext* context) {
	const std::string active(PanelState::Active());
	if (active == g_lastPanel) return;
	g_lastPanel = active;
	if (active.empty()) return;

	// The character panel has its own reader, which names the character behind
	// the title and keeps every statistic on the right stick. Read from here it
	// was thirty-odd bare labels and numbers in one breath (MEASURED
	// 2026-09-21), cut off in turn by the tab reader.
	if (CharacterStatsReader::Owns(active)) {
		AsyncLog::InfoF(context, "[modal] %s is read by its own reader", active.c_str());
		return;
	}

	// A panel the player can navigate is the focus reader's job; this is only
	// for the ones the game opens without giving anything focus.
	if (NativeUi::CurrentFocus().widget != 0) return;

	const uintptr_t panel = NativeWidgets::FindPanel(active);
	if (panel == 0) return;

	// Read the LIVE tree, not the catalogue. The catalogue is generated from
	// the layout JSON and can simply lack a control: DeleteCharacterModal is
	// catalogued as Confirm, ConfirmLegend, Dismiss and DismissLegend, with no
	// entry at all for Prompt -- the TextBoxWidget that holds the message. A
	// catalogue-driven read could never reach it, however many widget types it
	// was taught to look for, which is why this panel stayed silent while
	// reporting that it had found both of its legends.
	const UiText::Snapshot snapshot = NativeUi::Snapshot(panel, 0);
	// Inventory panels briefly lose focus when tabs change; their labels are
	// not a modal message. The shared item reader will announce the next focus.
	if (HasInventory(snapshot)) return;
	std::string            phrase;
	for (const UiText::Node& node : snapshot.nodes) {
		if (!Speakable(node) || UnderTabBar(snapshot, node.parent)) continue;
		const auto text = UiText::Clean(node.text);
		if (text.empty()) continue;
		AsyncLog::InfoF(context, "[modal] %s/%s (%s) -> \"%s\"", active.c_str(), node.name.c_str(), node.type.c_str(), text.c_str());
		Append(phrase, text);
	}
	if (phrase.empty()) {
		AsyncLog::InfoF(context, "[modal] %s: %zu nodes, none with speakable text", active.c_str(), snapshot.nodes.size());
		return;
	}
	// Queued, never interrupting: this is what the panel says AFTER its title.
	// The window announcer speaks that title the moment the panel opens, with
	// interrupt, so nothing stale is left ahead of this -- cutting in here only
	// ever cut the title off. MEASURED 2026-09-21: "Quest Log" was interrupted
	// 42 ms after it began by this reader speaking the act tabs' "I".
	ScreenReader::Speak(phrase, false);
}

}
