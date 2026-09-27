#include "skill_navigation.h"
#include "skill_grid.h"
#include "input_direction.h"
#include "native_skill_navigation.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "progression_text.h"
#include "async_log.h"
#include <algorithm>
#include <atomic>
#include <vector>

namespace D2RAccess::SkillNavigation {
namespace {
std::atomic<bool> g_enabled{true};
uintptr_t g_panel = 0, g_selected = 0;
bool g_gesture = false;

}
std::vector<SkillGrid::Node> Nodes(uintptr_t panel) {
	std::vector<SkillGrid::Node> result;
	for (const auto button : NativeUi::FindControls(panel, UiText::Kind::Button)) {
		if (!NativeUi::Visible(button)) continue;
		const auto container = NativeUi::Parent(button);
		const auto name = NativeWidgets::ReadName(container);
		NativeUi::Cell cell;
		if (name == "SkillButtonContainer") cell = ProgressionText::SkillCell(NativeWidgets::ReadName(button));
		else if (name == "CommonSkillsContainer" || name == "ItemSkillsContainer") cell = NativeSkillNavigation::Position(button);
		else continue;
		if (cell.x >= 0 && cell.y >= 0) result.push_back({button, cell.x, cell.y});
	}
	return result;
}
void Reset() {
	NativeSkillNavigation::HoldCursor(false);
	g_panel = g_selected = 0;
	g_gesture = false;
}
void SetEnabled(bool enabled) { g_enabled = enabled; }
void Tick(const D2RL::PluginContext* context) {
	if (!g_enabled || !NativeSkillNavigation::Available()) { Reset(); return; }
	const auto panel = NativeWidgets::FindPanel("SkillsTreePanel");
	const auto focus = NativeUi::CurrentFocus();
	if (!panel || !NativeUi::Visible(panel) || focus.panel != panel) { Reset(); return; }
	if (g_panel != panel) { Reset(); g_panel = panel; }
	const auto nodes = Nodes(panel);
	if (nodes.empty()) { Reset(); return; }
	const auto contains = [&nodes](uintptr_t widget) {
		return std::any_of(nodes.begin(), nodes.end(), [widget](const auto& n) { return n.widget == widget; });
	};
	const auto input = InputDirection::Dpad();
	if (!contains(g_selected)) g_selected = contains(focus.widget) ? focus.widget : 0;
	if (!input.held) {
		g_gesture = false;
		NativeSkillNavigation::HoldCursor(false);
		if (contains(focus.widget)) g_selected = focus.widget;
		return;
	}
	// A direction/diagonal transition while still held belongs to the same
	// physical gesture. Only a full release rearms another grid movement.
	if (input.pressed && !g_gesture) {
		const auto next = SkillGrid::Move(nodes, g_selected, input.x, input.y);
		if (next) {
			NativeSkillNavigation::HoldCursor(true);
			if (NativeSkillNavigation::Select({panel, next})) {
				g_selected = next;
				g_gesture = true;
				AsyncLog::InfoF(context, "[skill-grid] Selected %s", NativeWidgets::ReadName(next).c_str());
			} else {
				g_gesture = false;
				AsyncLog::Warn(context, "[skill-grid] Selection unavailable; cursor navigation retained.");
			}
		}
	}
	NativeSkillNavigation::HoldCursor(g_gesture);
}

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled() { return g_enabled; }
}
