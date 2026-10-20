#include "stash_pages.h"
#include "native_stash_pages.h"
#include "native_widgets.h"
#include "native_skill_navigation.h"
#include "ui_text.h"

namespace D2RAccess::StashPages {
namespace {
// BankExpansionLayoutHD.json: the Shared tab reveals this container. Its
// current page is rendered between the same two buttons a mouse user clicks.
// A translated caption is not used to identify either the tab or its controls.
uintptr_t Navigation() {
	uintptr_t node = NativeWidgets::FindPanel("BankExpansionLayout");
	if (!node || !NativeUi::Visible(node)) return 0;
	for (const auto name : {"basicstash_container", "SharedStashTabContainer", "StashNavigation"}) {
		node = NativeWidgets::FindChild(node, name);
		if (!node || !NativeUi::Visible(node)) return 0;
	}
	return node;
}

std::string Page(uintptr_t navigation) {
	if (!navigation) return {};
	const auto text = NativeWidgets::FindChild(navigation, "CurrentSharedStashText");
	if (!text || !NativeUi::Visible(text)) return {};
	auto label = UiText::Clean(NativeWidgets::ReadText(text));
	return !label.empty() && !label.starts_with('@') ? label : std::string{};
}
}

bool Available() { return !Page(Navigation()).empty(); }
std::string Label() { return Page(Navigation()); }
bool Change(int delta, int pages) {
	if (Page(Navigation()).empty()) return false;
	const auto panel = NativeWidgets::FindPanel("BankExpansionLayout");
	return panel && NativeUi::Visible(panel) && NativeStashPages::Change(panel, delta, pages);
}

std::optional<NativeUi::Rect> Arrow(int delta) {
	if (delta != -1 && delta != 1) return {};
	const auto navigation = Navigation();
	if (Page(navigation).empty()) return {};
	const auto button = NativeWidgets::FindChild(navigation, delta < 0 ? "StashLeftArrow" : "StashRightArrow");
	// Controller legend images are not mouse buttons. No fallback clicks on
	// unrelated menus, hidden Shared tabs, or a stale page after a tab switch.
	if (!button || !NativeUi::Visible(button) || NativeUi::Classify(button).kind != UiText::Kind::Button) return {};
	return NativeSkillNavigation::RectAt(button, NativeUi::FocusSelection(button));
}
}
