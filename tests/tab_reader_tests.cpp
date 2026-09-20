#include "tab_reader.h"
#include "test_support.h"

#include <unordered_map>
#include <unordered_set>

namespace {
using namespace D2RAccess;
std::vector<uintptr_t> panels{10, 20, 30}; // stash, focused inventory, controller overlay
std::unordered_map<uintptr_t, std::vector<uintptr_t>> controls{{10, {100, 101}}, {20, {}}, {30, {300}}};
std::unordered_map<uintptr_t, std::string> labels{{100, "Personal"}, {101, "1"}, {300, "Inventario"}};
std::unordered_set<uintptr_t> hidden{101};
// Only the controller overlay's bar reports a position; the others stand for
// bars whose children cannot be counted, which must still announce the name.
std::unordered_map<uintptr_t, std::pair<int, int>> positions{{300, {2, 4}}};
int walks = 0;
std::string Tick(bool suspended = false) { return TabReader::Update(panels, {20, 21}, suspended); }
}

namespace D2RAccess::NativeUi {
bool Visible(uintptr_t widget) { return !hidden.contains(widget); }
Type Classify(uintptr_t widget) { return {"AnyPanel", UiText::Kind::Other, widget == 40 ? UiText::Surface::Narrative : UiText::Surface::Other}; }
std::vector<uintptr_t> FindControls(uintptr_t panel, UiText::Kind kind) {
	++walks;
	Check(kind == UiText::Kind::Tabs, "shared discovery requests the tab archetype");
	return controls[panel];
}
std::string SelectedTabText(uintptr_t widget) { return Visible(widget) ? labels[widget] : ""; }
Tab SelectedTab(uintptr_t widget) {
	const auto it = positions.find(widget);
	if (!Visible(widget) || it == positions.end()) return {};
	return {it->second.first, it->second.second, labels[widget]};
}
}

int main() {
	using namespace D2RAccess;
	Check(Tick().empty(), "initial tabs establish a baseline without repeating the menu title");
	Check(TabReader::Selected(10) == "Personal" && TabReader::Selected(20).empty(), "stash context belongs to the stash, not the focused player inventory");
	labels[100] = "Comp.";
	Check(Tick() == "Comp.", "LT/RT changing an unfocused stash tab is announced immediately");
	Check(Tick().empty(), "unchanged selected tabs stay silent");
	labels[300] = "Árbol de hab.";
	Check(Tick() == "Árbol de hab., tab 3 of 4", "a tab bar says where along the strip the highlight landed");
	labels[300] = "Opciones";
	Check(Tick() == "Opciones, tab 3 of 4", "consecutive tab changes do not require moving item focus");
	labels[300] = "Inventario";
	Check(Tick() == "Inventario, tab 3 of 4", "returning to the original tab is announced again");
	positions[300] = {0, 1};
	labels[300] = "Solo";
	Check(Tick() == "Solo", "a bar with a single tab is a label, not a position");
	positions[300] = {2, 4};
	hidden.erase(101);
	Check(Tick() == "1" && TabReader::Selected(10) == "Comp.. 1", "newly visible shared-stash sub-tabs are read without a tree rebuild");
	Check(walks == 3, "steady navigation never rescans inventory trees");
	hidden.insert(101); labels[101] = "2";
	Check(Tick().empty(), "hidden sub-tabs do not announce changes");
	hidden.insert(10);
	Check(Tick().empty() && TabReader::Selected(10).empty(), "closing a panel removes its cached controls");
	hidden.erase(10); controls[10] = {102}; labels[102] = "Personal";
	Check(Tick() == "Personal" && TabReader::Selected(10) == "Personal", "reopening a rebuilt panel discovers its new controls");
	labels[102] = "\xEE\x80\x93";
	Check(Tick().empty(), "controller glyphs do not become tab labels");
	Check(Tick(true).empty() && TabReader::Selected(30).empty(), "dialogues suspend and reset tab tracking");
	Check(Tick().empty(), "return from dialogue silently establishes current tabs");
	TabReader::Reset(); panels = {40}; controls[40] = {400}; labels[400] = "Unrelated";
	const int priorWalks = walks;
	Check(Tick().empty() && walks == priorWalks, "narrative panels are not scanned for background navigation tabs");
	TabReader::Reset();
	Passed();
}
