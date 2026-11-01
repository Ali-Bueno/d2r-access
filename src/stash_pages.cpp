#include "stash_pages.h"
#include "native_stash_pages.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "ui_text.h"

namespace D2RAccess::StashPages {
namespace {
// BankExpansionLayoutHD.json: the Shared tab reveals this chain; the page is the text between the arrows.
constexpr const char* PanelName = "BankExpansionLayout";
constexpr const char* PageChain[] = {"basicstash_container", "SharedStashTabContainer", "StashNavigation", "CurrentSharedStashText"};

// Nonempty only while every level is shown; an unresolved layout key ("@...") is no page.
std::string Page(uintptr_t panel) {
	uintptr_t node = panel;
	for (const auto name : PageChain) {
		node = NativeWidgets::FindChild(node, name);
		if (!node || !NativeUi::Visible(node)) return {};
	}
	auto label = UiText::Clean(NativeWidgets::ReadText(node));
	return !label.empty() && !label.starts_with('@') ? label : std::string{};
}
uintptr_t Panel() {
	const auto panel = NativeWidgets::FindPanel(PanelName);
	return panel && NativeUi::Visible(panel) ? panel : 0;
}
}

std::string Label() { return Page(Panel()); }
int Change(int direction, int pages) {
	const auto panel = Panel();
	return panel && !Page(panel).empty() ? NativeStashPages::Change(panel, direction, pages) : -1;
}
}
