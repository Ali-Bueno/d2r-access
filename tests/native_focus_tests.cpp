#include "native_focus.h"
#include "native_widgets.h"
#include "test_support.h"
#include <map>

namespace {
struct Widget { uintptr_t parent; const char* name; bool visible = true; };
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
}
namespace D2RAccess::NativeWidgets {
std::string ReadName(uintptr_t id) { return widgets.contains(id) ? widgets.at(id).name : ""; }
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
	Passed();
}
