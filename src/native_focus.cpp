#include "native_focus.h"
#include "native_widgets.h"

namespace D2RAccess::NativeFocus {
namespace {
// How many run-time containers may sit between two levels the layout names.
// Four covers a scroll view's viewport with room to spare, and still stops a
// mismatched widget from climbing to the panel and claiming a wrong entry.
constexpr uint32_t MaxUndeclared = 4;

// Walks focus.widget up through the entry's declared path, tolerating
// undeclared levels between named ones. Returns the ancestor above the
// outermost named step, or 0 if the chain does not match.
uintptr_t Climb(uintptr_t widget, const Generated::WidgetEntry& entry) {
	uintptr_t ancestor = widget;
	for (uint32_t step = entry.pathLength; step > 0; --step) {
		const char* expected = entry.path[step - 1].name;
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
}

Match Resolve(NativeUi::Focus focus, std::span<const Generated::PanelWidgets> catalog) {
	if (!NativeUi::Visible(focus.panel) || !NativeUi::Visible(focus.widget)) return {};
	const auto panelName = NativeWidgets::ReadName(focus.panel);
	const auto widgetName = NativeWidgets::ReadName(focus.widget);
	for (const auto& panel : catalog) {
		if (panelName != panel.panel) continue;
		for (uint32_t i = 0; i < panel.count; ++i) {
			const auto& widget = panel.widgets[i];
			if (widgetName != widget.name || !widget.path || !widget.pathLength) continue;
			// The layout declares the authored tree, but a ScrollViewWidget builds
			// its own viewport at run time: live is ScrollView / View / OptionsTable
			// where the layout says ScrollView / OptionsTable. Requiring each step to
			// be the immediate parent therefore failed every widget inside a
			// scrolling page, which is every options pane. So a step may sit above a
			// bounded number of undeclared containers -- bounded, so a widget cannot
			// match some other entry by climbing far enough to reach the panel.
			uintptr_t ancestor = Climb(focus.widget, widget);
			for (uint32_t skipped = 0; ancestor && ancestor != focus.panel && skipped < MaxUndeclared; ++skipped) {
				ancestor = NativeUi::Parent(ancestor);
			}
			if (ancestor && ancestor == focus.panel) return {&panel, &widget};
		}
	}
	return {};
}
}
