#include "menus/stash_pages.h"
#include "menus/native_stash_pages.h"
#include "native_widgets.h"
#include "native_skill_navigation.h"
#include "test_support.h"
#include <map>

using namespace D2RAccess;
namespace {
struct Node { uintptr_t parent; std::string name, text; UiText::Kind kind = UiText::Kind::Other; bool visible = true; };
// Original mod-owned synthetic UI, not a copied game implementation or memory dump.
std::map<uintptr_t, Node> nodes;
bool hasRect = true;
int rectangleReads = 0;
int pageCalls = 0, pageDirection = 0, pageAmount = 0;
void Restore() {
	nodes = {
		{1, {0, "BankExpansionLayout", ""}}, {2, {1, "basicstash_container", ""}},
		{3, {2, "SharedStashTabContainer", ""}}, {4, {3, "StashNavigation", ""}},
		{5, {4, "CurrentSharedStashText", "Page 2 / 5", UiText::Kind::Text}},
		{6, {4, "StashLeftArrow", "", UiText::Kind::Button}},
		{7, {4, "StashRightArrow", "", UiText::Kind::Button}},
	};
	hasRect = true; rectangleReads = 0; pageCalls = 0;
}
}
namespace D2RAccess::NativeStashPages {
bool Change(uintptr_t panel, int direction, int pages) {
	Check(panel == 1, "page command receives the freshly resolved owning panel");
	++pageCalls; pageDirection = direction; pageAmount = pages; return true;
}
}
namespace D2RAccess::NativeWidgets {
uintptr_t FindPanel(std::string_view name) { return nodes.contains(1) && nodes[1].name == name ? 1 : 0; }
uintptr_t FindChild(uintptr_t parent, std::string_view name) {
	for (const auto& [id, node] : nodes) if (node.parent == parent && node.name == name) return id;
	return 0;
}
std::string ReadText(uintptr_t node) { return nodes.contains(node) ? nodes[node].text : std::string{}; }
}
namespace D2RAccess::NativeUi {
bool Visible(uintptr_t node) {
	for (size_t depth = 0; node && depth < 8; ++depth) {
		if (!nodes.contains(node) || !nodes[node].visible) return false;
		node = nodes[node].parent;
		if (!node) return true;
	}
	return false;
}
Type Classify(uintptr_t node) { return nodes.contains(node) ? Type{"", nodes[node].kind} : Type{}; }
uint64_t FocusSelection(uintptr_t) { return 0; }
}
namespace D2RAccess::NativeSkillNavigation {
std::optional<NativeUi::Rect> RectAt(uintptr_t node, uint64_t) {
	++rectangleReads;
	if (!hasRect || (node != 6 && node != 7)) return {};
	return NativeUi::Rect{node == 6 ? 10 : 300, 20, 64, 64};
}
}
int main() {
	Restore();
	Check(StashPages::Available() && StashPages::Label() == "Page 2 / 5", "visible Shared page provides its rendered label");
	Check(StashPages::Arrow(-1) == NativeUi::Rect{10, 20, 64, 64}, "previous page resolves the existing left arrow");
	Check(StashPages::Arrow(1) == NativeUi::Rect{300, 20, 64, 64}, "next page resolves the existing right arrow");
	for (const int pages : {1,5,10}) {
		Check(StashPages::Change(1, pages) && pageDirection == 1 && pageAmount == pages, "native amount is copied independently of physical Shift/Ctrl");
		Check(StashPages::Change(-1, pages) && pageDirection == -1 && pageAmount == pages, "all amounts support previous direction");
	}
	nodes[5].text = "Página 3 / 5";
	Check(StashPages::Label() == "Página 3 / 5", "page speech remains the game's current localized text");
	nodes[3].visible = false;
	Check(!StashPages::Available() && !StashPages::Arrow(1), "another stash tab cannot receive a shared-page click");
	const int before = pageCalls;
	Check(!StashPages::Change(1, 10) && pageCalls == before, "stale context cannot reach native page command");
	Check(rectangleReads == 2, "hidden page controls never reach the native rectangle getter");
	nodes[3].visible = true;
	nodes[7].kind = UiText::Kind::Legend;
	Check(!StashPages::Arrow(1), "controller legend images are never treated as clickable buttons");
	nodes[7].kind = UiText::Kind::Button; nodes[7].visible = false;
	Check(!StashPages::Arrow(1), "a hidden arrow is not clicked");
	nodes[7].visible = true; hasRect = false;
	Check(!StashPages::Arrow(1), "unavailable native geometry fails safely");
	Restore(); nodes[5].text = "@SharedStashPage";
	Check(!StashPages::Available() && !StashPages::Arrow(-1), "an unresolved layout key does not establish page readiness");
	Restore(); nodes.erase(5);
	Check(!StashPages::Available(), "missing current-page text does not establish page readiness");
	Restore(); nodes[1].visible = false;
	Check(!StashPages::Available() && !StashPages::Arrow(-1), "closing the stash immediately retires its page controls");
	Restore(); nodes[1].name = "VendorPanelLayout";
	Check(!StashPages::Available() && !StashPages::Arrow(1), "other menus keep the existing comparison keys");
	Restore();
	Check(!StashPages::Arrow(0) && !StashPages::Arrow(2), "invalid page directions never reach a native getter");
	Check(rectangleReads == 0, "invalid requests perform no rectangle reads");
	Passed();
}
