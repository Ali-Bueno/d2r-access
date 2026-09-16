#include "native_focus.h"
#include "native_widgets.h"

namespace D2RAccess::NativeFocus {
Match Resolve(NativeUi::Focus focus, std::span<const Generated::PanelWidgets> catalog) {
	if (!NativeUi::Visible(focus.panel) || !NativeUi::Visible(focus.widget)) return {};
	const auto panelName = NativeWidgets::ReadName(focus.panel);
	const auto widgetName = NativeWidgets::ReadName(focus.widget);
	for (const auto& panel : catalog) {
		if (panelName != panel.panel) continue;
		for (uint32_t i = 0; i < panel.count; ++i) {
			const auto& widget = panel.widgets[i];
			if (widgetName != widget.name || !widget.path || !widget.pathLength) continue;
			uintptr_t ancestor = focus.widget;
			for (uint32_t step = widget.pathLength; step > 0; --step) {
				if (!ancestor || NativeWidgets::ReadName(ancestor) != widget.path[step - 1].name) { ancestor = 0; break; }
				ancestor = NativeUi::Parent(ancestor);
			}
			if (ancestor == focus.panel) return {&panel, &widget};
		}
	}
	return {};
}
}
