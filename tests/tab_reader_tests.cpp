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
std::unordered_map<uintptr_t, int> indices;
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
int SelectedTab(uintptr_t widget) { return indices[widget]; }
std::string SelectedTabText(uintptr_t widget) { return Visible(widget) ? labels[widget] : ""; }
}
namespace D2RAccess::NativeWidgets {
std::string ReadName(uintptr_t widget) { return widget == 300 ? "UISwitcherTabs" : "TabBar"; }
}

int main() {
	using namespace D2RAccess;
	Check(Tick().empty(), "initial tabs establish a baseline without repeating the menu title");
	Check(TabReader::Selected(10) == "Personal" && TabReader::Selected(20).empty(), "stash context belongs to the stash, not the focused player inventory");
	labels[100] = "Comp.";
	Check(Tick() == "Comp.", "LT/RT changing an unfocused stash tab is announced immediately");
	Check(Tick().empty(), "unchanged selected tabs stay silent");
	labels[300] = "Árbol de hab.";
	Check(Tick() == "Árbol de hab.", "controller overlay tabs outside both inventory panels are announced");
	labels[300] = "Opciones";
	Check(Tick() == "Opciones", "consecutive tab changes do not require moving item focus");
	labels[300] = "Inventario";
	Check(Tick() == "Inventario", "returning to the original tab is announced again");
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
	Check(Tick(true).empty() && TabReader::Selected(30) == "Inventario", "a dialogue keeps the tab baseline");
	Check(Tick().empty(), "return from dialogue with nothing changed stays silent");
	// The quest log is a switcher tab that suspends this reader; the tab after
	// it -- Options, which has no window title -- is still named.
	Check(Tick(true).empty(), "the quest log suspends tab tracking");
	labels[300] = "Opciones";
	Check(Tick() == "Opciones", "the switcher tab reached from a dialogue is named on return");
	Check(Tick().empty(), "and only once");
	// A host's log, 2026-10-12: Party, then the quest log (a dialogue), then
	// Character. The bar still showed the quest log a poll after Character opened,
	// and "Quest Log" cut off the window's "Character".
	labels[300] = "Grupo"; indices[300] = 0;
	(void)Tick();
	labels[300] = "Reg. de búsquedas"; indices[300] = 1;
	Check(Tick(true).empty(), "the quest log holds the switcher");
	Check(Tick().empty(), "the bar still on the quest log once it closed is not named");
	labels[300] = "Personaje"; indices[300] = 2;
	Check(Tick() == "Personaje", "the tab the bar really moves to next is");
	indices[300] = 0;
	Check(Tick(true).empty(), "a dialogue suspends tracking again");
	labels[102] = "Comp.";
	Check(Tick() == "Comp.", "other tab bars are still read after a dialogue (vendor tabs)");
	labels[102] = "Weapon"; indices[102] = 1;
	Check(Tick(true) == "Weapon", "a bar changed while a readout owns speech is still announced");
	labels[102] = "Weapon"; indices[102] = 2;
	Check(Tick(true) == "Weapon", "a repeated caption on another index is announced");
	Check(Tick(true).empty() && TabReader::Detail().find("idx=2") != std::string::npos, "an unchanged bar stays silent and logs its index");
	// A tester's log, 2026-10-18: the vendor opens behind the trade menu, which still reads.
	panels.push_back(50); controls[50] = {500}; labels[500] = "Misc";
	Check(Tick(true).empty(), "a vendor bar that opens under a conversation is its baseline");
	labels[500] = "Armor"; indices[500] = 0;
	Check(Tick(true) == "Armor", "LT/RT on it before the cursor lands is announced");
	panels.pop_back();
	TabReader::Reset(); panels = {40}; controls[40] = {400}; labels[400] = "Unrelated";
	const int priorWalks = walks;
	Check(Tick().empty() && walks == priorWalks, "narrative panels are not scanned for background navigation tabs");
	TabReader::Reset();
	Passed();
}
