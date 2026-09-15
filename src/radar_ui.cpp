#include "radar_ui.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"

#include <algorithm>

namespace D2RAccess::RadarUi {
State Inspect(bool foreground) {
	if (!foreground) return State::Inactive;
	if (!NativeWidgets::SupportsTree()) return State::Unavailable;
	const auto& open = PanelState::OpenPanels();
	for (const auto panel : NativeUi::Panels()) {
		const auto name = NativeWidgets::ReadName(panel);
		// Panels() enumerates loaded panels too. Their widget visibility does
		// not establish that PanelManager has opened them for gameplay.
		if (std::find(open.begin(), open.end(), name) == open.end() || !NativeUi::Visible(panel)) continue;
		const auto surface = NativeUi::Classify(panel).surface;
		if (surface == UiText::Surface::Menu || surface == UiText::Surface::Narrative) return State::Dialogue;
		if (name.starts_with("PauseLayout") || name == "SettingsPanel") return State::Paused;
	}
	return State::Playing;
}
}
