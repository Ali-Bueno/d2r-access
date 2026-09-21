#include "modal_reader.h"

#include "async_log.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "ui_text.h"

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
	std::string            phrase;
	for (const UiText::Node& node : snapshot.nodes) {
		if (!Speakable(node)) continue;
		AsyncLog::InfoF(context, "[modal] %s/%s (%s) -> \"%s\"", active.c_str(), node.name.c_str(), node.type.c_str(), node.text.c_str());
		Append(phrase, node.text);
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
