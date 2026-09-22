#include "native_focus.h"
#include "native_widgets.h"
#include "test_support.h"
#include <map>

namespace {
struct Widget { uintptr_t parent; const char* name; bool visible = true; D2RAccess::NativeUi::Rect rect{}; };
std::map<uintptr_t, Widget> widgets;
}
namespace D2RAccess::NativeUi {
bool Visible(uintptr_t id) {
	for (size_t depth = 0; depth < widgets.size(); ++depth) {
		const auto it = widgets.find(id);
		if (it == widgets.end() || !it->second.visible) return false;
		if (!it->second.parent) return true;
		id = it->second.parent;
	}
	return false;
}
uintptr_t Parent(uintptr_t id) { return widgets.contains(id) ? widgets.at(id).parent : 0; }
std::optional<Rect> LocalRect(uintptr_t id) { return widgets.contains(id) ? std::optional<Rect>{widgets.at(id).rect} : std::nullopt; }
std::vector<uintptr_t> Children(uintptr_t parent) {
	std::vector<uintptr_t> result;
	for (const auto& [id, widget] : widgets) if (widget.parent == parent) result.push_back(id);
	return result;
}
}
namespace D2RAccess::NativeWidgets {
std::string ReadName(uintptr_t id) { return widgets.contains(id) ? widgets.at(id).name : ""; }
uintptr_t FindChild(uintptr_t parent, std::string_view name) {
	for (const auto& [id, widget] : widgets) if (widget.parent == parent && widget.name == name) return id;
	return 0;
}
}

int main() {
	using namespace D2RAccess;
	widgets = {{1, {0, "MainMenuButtonRibbon"}}, {2, {1, "Ribbon"}},
		{3, {2, "Settings"}}, {4, {2, "Cinematics"}}, {5, {2, "Credits"}}, {6, {2, "Exit"}}};
	for (uintptr_t id = 3; id <= 6; ++id) {
		const auto match = NativeFocus::Resolve({1, id}, Generated::PanelCatalog);
		Check(match.widget && std::string_view(match.widget->name) == widgets.at(id).name,
			"native ribbon focus identifies options/videos/credits/exit without any rectangle or panel stack");
	}
	widgets[6].visible = false;
	Check(!NativeFocus::Resolve({1, 6}, Generated::PanelCatalog).widget, "hidden controls never match");
	widgets[6].visible = true; widgets[2].visible = false;
	Check(!NativeFocus::Resolve({1, 6}, Generated::PanelCatalog).widget, "covered panel ancestry prevents a match");
	widgets[2].visible = true;
	widgets[7] = {1, "OtherRibbon"}; widgets[8] = {7, "Settings"};
	Check(!NativeFocus::Resolve({1, 8}, Generated::PanelCatalog).widget, "identical names under different parents do not alias");
	widgets[9] = {0, "MainMenuButtonRibbon"};
	Check(!NativeFocus::Resolve({9, 3}, Generated::PanelCatalog).widget, "focused widget must belong to the actual panel instance");
	widgets[1].name = "UnknownPanel";
	Check(!NativeFocus::Resolve({1, 3}, Generated::PanelCatalog).widget, "unknown native controls leave the existing fallback available");
	Check(!NativeFocus::Resolve({}, Generated::PanelCatalog).widget, "unavailable focus does not fabricate a selection");

	// Live character rows replace the authored CharacterTemplate with numbered
	// clones beneath a runtime scroll viewport (2026-09-22 focus log).
	widgets = {{20, {0, "CharacterSelectPanel"}}, {21, {20, "Background"}}, {22, {21, "CharacterList"}},
		{23, {22, "View"}}, {24, {23, "Container"}}, {25, {24, "ListItem0"}}, {26, {24, "ListItem1"}}};
	for (uintptr_t id : {25u, 26u}) {
		const auto match = NativeFocus::Resolve({20, id}, Generated::PanelCatalog);
		Check(match.widget && std::string_view(match.widget->name) == "CharacterTemplate" && match.nativeWidget == id,
			"a numbered character row clone resolves to the template and returns the actual focused row");
	}
	widgets[24].rect = {0, -20, 0, 0};
	widgets[25].rect = {0, 20, 612, 184}; widgets[26].rect = {0, 204, 612, 184};
	widgets[28] = {23, "Selection", true, {0, 184, 612, 184}};
	Check(NativeFocus::SelectedCharacterRow(25) == 26, "selection rect resolves the highlighted clone through a shifted scroll container");
	widgets[28].visible = false;
	Check(NativeFocus::SelectedCharacterRow(25) == 25, "a hidden selector leaves native focus unchanged");
	widgets[28].visible = true; widgets[26].visible = false;
	Check(NativeFocus::SelectedCharacterRow(25) == 25, "a hidden selected row leaves native focus unchanged");
	widgets[26].visible = true; widgets[27] = {24, "ListItem2", true, widgets[26].rect};
	Check(NativeFocus::SelectedCharacterRow(25) == 25, "overlapping row bounds are ambiguous and leave native focus unchanged");
	widgets.erase(27); widgets[28].rect.y = 999;
	Check(NativeFocus::SelectedCharacterRow(25) == 25, "a selector outside every row leaves native focus unchanged");
	widgets[27] = {24, "ListItemX"};
	Check(!NativeFocus::Resolve({20, 27}, Generated::PanelCatalog).widget, "only numbered character row clones alias the template");
	const Generated::WidgetEntry malformed{"CharacterTemplate", "ListItemWidget", nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr, nullptr, 0, nullptr, 2};
	const Generated::PanelWidgets malformedPanel{"CharacterSelectPanel", 0.0f, 0.0f, &malformed, 1};
	Check(!NativeFocus::Resolve({20, 25}, {&malformedPanel, 1}).widget, "a malformed runtime-row catalog entry fails closed");
	Passed();
}
