#include "native_focus.h"
#include "native_widgets.h"

#include <cctype>
#include <cstring>

namespace D2RAccess::NativeFocus {
namespace {
// How many run-time containers may sit between two levels the layout names.
// Four covers a scroll view's viewport with room to spare, and still stops a
// mismatched widget from climbing to the panel and claiming a wrong entry.
constexpr uint32_t MaxUndeclared = 4;

// Walks focus.widget up through the entry's declared path, tolerating
// undeclared levels between named ones. Returns the ancestor above the
// outermost named step, or 0 if the chain does not match.
uintptr_t Climb(uintptr_t widget, const Generated::WidgetEntry& entry, bool runtimeInnermost = false) {
	uintptr_t ancestor = widget;
	for (uint32_t step = entry.pathLength; step > 0; --step) {
		const char* expected = entry.path[step - 1].name;
		if (runtimeInnermost && step == entry.pathLength) {
			ancestor = NativeUi::Parent(ancestor);
			continue;
		}
		// The innermost step is the focused widget itself, already matched by
		// name, so only the levels above it may be skipped over.
		for (uint32_t skipped = 0; ancestor && NativeWidgets::ReadName(ancestor) != expected; ++skipped) {
			if (step == entry.pathLength || skipped >= MaxUndeclared) return 0;
			ancestor = NativeUi::Parent(ancestor);
		}
		if (!ancestor) return 0;
		ancestor = NativeUi::Parent(ancestor);
	}
	return ancestor;
}

bool NumberedRow(std::string_view name) {
	constexpr std::string_view Prefix = "ListItem";
	if (!name.starts_with(Prefix) || name.size() == Prefix.size()) return false;
	for (const unsigned char c : name.substr(Prefix.size())) if (!std::isdigit(c)) return false;
	return true;
}

bool CharacterRowClone(const Generated::PanelWidgets& panel, const Generated::WidgetEntry& entry, const std::string& actualName) {
	constexpr const char* Panel = "CharacterSelectPanel";
	constexpr const char* List = "CharacterList";
	constexpr const char* Template = "CharacterTemplate";
	if (std::strcmp(panel.panel, Panel) != 0 || std::strcmp(entry.name, Template) != 0 || !entry.path || entry.pathLength < 2
		|| std::strcmp(entry.path[entry.pathLength - 2].name, List) != 0) return false;
	return NumberedRow(actualName);
}
}

Match Resolve(NativeUi::Focus focus, std::span<const Generated::PanelWidgets> catalog) {
	if (!NativeUi::Visible(focus.panel) || !NativeUi::Visible(focus.widget)) return {};
	const auto panelName = NativeWidgets::ReadName(focus.panel);
	const auto widgetName = NativeWidgets::ReadName(focus.widget);
	for (const auto& panel : catalog) {
		if (panelName != panel.panel) continue;
		for (uint32_t i = 0; i < panel.count; ++i) {
			const auto& widget = panel.widgets[i];
			const bool runtimeCharacterRow = CharacterRowClone(panel, widget, widgetName);
			if ((widgetName != widget.name && !runtimeCharacterRow) || !widget.path || !widget.pathLength) continue;
			// The layout declares the authored tree, but a ScrollViewWidget builds
			// its own viewport at run time: live is ScrollView / View / OptionsTable
			// where the layout says ScrollView / OptionsTable. Requiring each step to
			// be the immediate parent therefore failed every widget inside a
			// scrolling page, which is every options pane. So a step may sit above a
			// bounded number of undeclared containers -- bounded, so a widget cannot
			// match some other entry by climbing far enough to reach the panel.
			uintptr_t ancestor = Climb(focus.widget, widget, runtimeCharacterRow);
			for (uint32_t skipped = 0; ancestor && ancestor != focus.panel && skipped < MaxUndeclared; ++skipped) {
				ancestor = NativeUi::Parent(ancestor);
			}
			if (ancestor && ancestor == focus.panel) return {&panel, &widget, focus.widget};
		}
	}
	return {};
}

uintptr_t SelectedCharacterRow(uintptr_t focusedRow) {
	if (!NumberedRow(NativeWidgets::ReadName(focusedRow))) return focusedRow;
	const uintptr_t container = NativeUi::Parent(focusedRow);
	const uintptr_t view = NativeUi::Parent(container);
	if (NativeWidgets::ReadName(container) != "Container" || NativeWidgets::ReadName(view) != "View"
		|| NativeWidgets::ReadName(NativeUi::Parent(view)) != "CharacterList") return focusedRow;
	const uintptr_t selection = NativeWidgets::FindChild(view, "Selection");
	const auto selected = NativeUi::LocalRect(selection);
	const auto containerRect = NativeUi::LocalRect(container);
	if (!selected || !containerRect || selected->width <= 0 || selected->height <= 0 || !NativeUi::Visible(selection)) return focusedRow;
	uintptr_t match = 0;
	for (const uintptr_t row : NativeUi::Children(container)) {
		if (!NumberedRow(NativeWidgets::ReadName(row))) continue;
		const auto rect = NativeUi::LocalRect(row);
		if (!rect || !NativeUi::Visible(row)) continue;
		if (static_cast<int64_t>(containerRect->x) + rect->x != selected->x
			|| static_cast<int64_t>(containerRect->y) + rect->y != selected->y
			|| rect->width != selected->width || rect->height != selected->height) continue;
		if (match) return focusedRow; // overlapping rows are not an identity
		match = row;
	}
	return match ? match : focusedRow;
}
}
