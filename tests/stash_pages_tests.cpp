#include "stash_pages.h"
#include "native_stash_pages.h"
#include "native_widgets.h"
#include "test_support.h"
#include <map>

using namespace D2RAccess;
namespace {
struct Node { uintptr_t parent; std::string name, text; bool visible = true; };
// Mod-owned synthetic widget tree.
std::map<uintptr_t, Node> nodes;
int pageCalls = 0, pageDirection = 0, pageAmount = 0;
void Restore() {
	nodes = {
		{1, {0, "BankExpansionLayout", ""}}, {2, {1, "basicstash_container", ""}},
		{3, {2, "SharedStashTabContainer", ""}}, {4, {3, "StashNavigation", ""}},
		{5, {4, "CurrentSharedStashText", "Page 2 / 5"}},
	};
	pageCalls = 0;
}
}
namespace D2RAccess::NativeStashPages {
int Change(uintptr_t panel, int direction, int pages) {
	Check(panel == 1, "the command receives the freshly resolved panel");
	++pageCalls; pageDirection = direction; pageAmount = pages; return pages;
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
	for (int depth = 0; node && depth < 8; ++depth) {
		if (!nodes.contains(node) || !nodes[node].visible) return false;
		node = nodes[node].parent;
		if (!node) return true;
	}
	return false;
}
}
int main() {
	Restore();
	Check(StashPages::Label() == "Page 2 / 5", "the visible Shared page gives its label");
	Check(StashPages::Change(1, 10) == 10 && pageDirection == 1 && pageAmount == 10, "forward by ten reaches the command");
	Check(StashPages::Change(-1, 1) == 1 && pageDirection == -1 && pageAmount == 1, "back by one reaches the command");
	nodes[5].text = "Pagina 3 / 5";
	Check(StashPages::Label() == "Pagina 3 / 5", "the label stays the game's localized text");
	nodes[3].visible = false;
	const int before = pageCalls;
	Check(StashPages::Label().empty() && StashPages::Change(1, 1) == -1 && pageCalls == before, "another stash tab cannot page");
	Restore(); nodes[5].text = "@SharedStashPage";
	Check(StashPages::Label().empty() && StashPages::Change(1, 1) == -1, "an unresolved layout key is no page");
	Restore(); nodes.erase(5);
	Check(StashPages::Label().empty(), "missing page text is no page");
	Restore(); nodes[1].visible = false;
	Check(StashPages::Label().empty() && StashPages::Change(1, 1) == -1, "a closed stash has no page");
	Restore(); nodes[1].name = "VendorPanelLayout";
	Check(StashPages::Label().empty() && StashPages::Change(-1, 1) == -1, "other menus have no page");
	Passed();
}
