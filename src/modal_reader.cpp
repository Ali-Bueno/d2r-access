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

	// A ButtonLegendWidget already carries the bound control IN CONTEXT -- the
	// glyph sits inside the authored wording. Appending a decoded control to
	// the prompt instead produces "...delete all versions of this character. A",
	// a dangling letter with no verb attached, so the legend is preferred
	// whole and the bare prompt is only the fallback for a panel that has none.
	std::string prompt;
	std::string legends;
	unsigned    legendWidgets = 0;
	for (uint32_t at = 0; at < catalog->count; ++at) {
		const Generated::WidgetEntry& widget = catalog->widgets[at];
		const bool isLegend = std::strcmp(widget.type, LegendType) == 0;
		const bool isText   = std::strcmp(widget.type, TextType) == 0;
		if (!isLegend && !isText) continue;
		const uintptr_t handle = NativeWidgets::Find(active, Steps(widget));
		if (handle == 0) {
			// Worth naming: a legend the catalogue has and the live tree does not
			// means the game built the non-controller variant of this panel, which
			// has no legends and expects a mouse -- and would explain a confirm
			// that no controller button can trigger.
			AsyncLog::InfoF(context, "[modal] %s/%s (%s) not present in the live tree", active.c_str(), widget.name, widget.type);
			continue;
		}
		if (isLegend) {
			++legendWidgets;
			const std::string text = NativeControllerHelp::Legend(handle);
			AsyncLog::InfoF(context, "[modal] %s/%s legend -> \"%s\"", active.c_str(), widget.name, text.c_str());
			Append(legends, text);
		} else {
			const std::string text = NativeWidgets::ReadText(handle);
			AsyncLog::InfoF(context, "[modal] %s/%s text -> \"%s\"", active.c_str(), widget.name, text.c_str());
			Append(prompt, text);
		}
	}
	AsyncLog::InfoF(context, "[modal] %s: %u legend widgets found", active.c_str(), legendWidgets);

	const std::string& phrase = legends.empty() ? prompt : legends;
	if (phrase.empty()) return;
	// A confirmation is a context change, and a destructive one should not wait
	// behind whatever was being said when it appeared.
	ScreenReader::Speak(phrase, true);
}

}
