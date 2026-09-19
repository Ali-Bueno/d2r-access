#include "modal_reader.h"

#include "async_log.h"
#include "generated/panel_widgets.h"
#include "native_controller_help.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"

#include <cstring>
#include <string>
#include <vector>

namespace D2RAccess::ModalReader {

namespace {

constexpr const char* LegendType = "ButtonLegendWidget";
constexpr const char* TextType   = "TextBoxWidget";

std::string g_lastPanel;

const Generated::PanelWidgets* Catalog(std::string_view panel) {
	for (const Generated::PanelWidgets& entry : Generated::PanelCatalog) {
		if (panel == entry.panel) return &entry;
	}
	return nullptr;
}

std::vector<std::string> Steps(const Generated::WidgetEntry& widget) {
	std::vector<std::string> path;
	path.reserve(widget.pathLength);
	for (uint32_t at = 0; at < widget.pathLength; ++at) path.emplace_back(widget.path[at].name);
	return path;
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

	const Generated::PanelWidgets* catalog = Catalog(active);
	if (catalog == nullptr) return;

	// The prompt first -- what is being asked -- then the legends, which name
	// the control that answers it. The legend text is read live rather than
	// from the string table, so it carries the button actually bound: a
	// hardcoded "press and hold" cannot say WHICH button, which is the only
	// part the player cannot work out for themselves.
	std::string prompt;
	std::string legends;
	for (uint32_t at = 0; at < catalog->count; ++at) {
		const Generated::WidgetEntry& widget = catalog->widgets[at];
		const bool isLegend = std::strcmp(widget.type, LegendType) == 0;
		const bool isText   = std::strcmp(widget.type, TextType) == 0;
		if (!isLegend && !isText) continue;
		const uintptr_t handle = NativeWidgets::Find(active, Steps(widget));
		if (handle == 0) continue;
		if (isLegend) {
			Append(legends, NativeControllerHelp::Legend(handle));
		} else {
			Append(prompt, NativeWidgets::ReadText(handle));
		}
	}
	if (prompt.empty() && legends.empty()) return;

	std::string phrase = prompt;
	Append(phrase, legends);
	AsyncLog::InfoF(context, "[modal] %s -> \"%s\"", active.c_str(), phrase.c_str());
	// A confirmation is a context change, and a destructive one should not wait
	// behind whatever was being said when it appeared.
	ScreenReader::Speak(phrase, true);
}

}
