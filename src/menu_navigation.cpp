#include "menu_navigation.h"
#include "menu_navigation_policy.h"
#include "input_direction.h"
#include "native_memory.h"
#include "native_skill_navigation.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "pad_typing.h"
#include "radar_config.h"
#include "skill_navigation.h"
#include "async_log.h"
#include <windows.h>
#include <algorithm>
#include <vector>

namespace D2RAccess::MenuNavigation {
namespace {
bool g_enabled = true, g_gesture = false;
uintptr_t g_panel = 0, g_selected = 0, g_restore = 0;

bool Under(const UiText::Snapshot& snapshot, size_t child, size_t ancestor) {
	for (int at = static_cast<int>(child); at >= 0;) {
		if (at == static_cast<int>(ancestor)) return true;
		const int parent = snapshot.nodes[static_cast<size_t>(at)].parent;
		if (parent >= at) break;
		at = parent;
	}
	return false;
}

std::vector<MenuNavigationPolicy::Node> Nodes(const UiText::Snapshot& snapshot) {
	size_t scope = 0;
	for (size_t i = 1; i < snapshot.nodes.size(); ++i)
		if (snapshot.nodes[i].name.ends_with("Modal")) scope = i;
	std::vector<MenuNavigationPolicy::Node> nodes;
	for (size_t i = scope; i < snapshot.nodes.size(); ++i) {
		const auto& node = snapshot.nodes[i];
		if (!Under(snapshot, i, scope) || !node.enabled) continue;
		using UiText::Kind;
		// List rows too: the browser's games (a tester, 2026-10-15: the pad could not reach them).
		if (node.kind != Kind::Button && node.kind != Kind::Toggle && node.kind != Kind::ListItem
			&& node.kind != Kind::TextInput && node.kind != Kind::ValueControl) continue;
		if (const auto rect = NativeSkillNavigation::FocusRect(node.id))
			nodes.push_back({node.id, rect->x, rect->y, rect->width, rect->height});
	}
	return nodes;
}
}

void Reset() {
	if (g_panel) NativeSkillNavigation::HoldCursor(false);
	PadTyping::Reset();
	g_panel = g_selected = g_restore = 0;
	g_gesture = false;
}
void Initialize(const D2RL::PluginContext* context) {
	Reset();
	g_enabled = RadarConfig::ReadEnabled(context, "[menu_navigation]").value_or(true);
}
bool Tick(const D2RL::PluginContext* context) {
	auto focus = NativeUi::CurrentFocus();
	// Loader-created forms use the stock travelling cursor: short D-pad presses
	// may not cross the gaps between fields. Other menus keep their native policy.
	if (!g_enabled || !NativeSkillNavigation::Available() || !focus.panel
		|| NativeWidgets::ReadName(focus.panel) != "D2RLoaderMultiplayerPanel") {
		Reset();
		return false;
	}
	if (g_panel != focus.panel) {
		SkillNavigation::Reset();
		Reset();
		g_panel = focus.panel;
		g_selected = focus.widget;
	}
	// The game keeps a box's editor after the focus leaves it: only the focused box counts
	// as typed in, or the D-pad is left to the stock cursor, which skips the boxes.
	auto editing = NativeSkillNavigation::TextEditing();
	if (editing && *editing) editing = NativeSkillNavigation::ActiveTextInput(focus.panel) == focus.widget;
	PadTyping::Tick(context, focus, editing, GetTickCount64());
	const auto input = InputDirection::Dpad();
	if (const auto restore = g_restore) {
		g_restore = 0;
		// Updating a form value can reset loader focus to the first input.
		// Restore only the control that emitted that value action, once.
		if (editing && !*editing && !input.pressed
			&& NativeUi::Classify(focus.widget).kind == UiText::Kind::TextInput) {
			const NativeMemory::ReadScope scope;
			const auto nodes = Nodes(NativeUi::Snapshot(focus.panel, focus.widget));
			if (std::any_of(nodes.begin(), nodes.end(), [restore](const auto& node) { return node.widget == restore; })
				&& NativeSkillNavigation::Select({focus.panel, restore})) {
				focus.widget = g_selected = restore;
				AsyncLog::InfoF(context, "[menu-navigation] Retained %s after value update", NativeWidgets::ReadName(restore).c_str());
			}
		}
	}
	// In the box being typed in, left and right stay with its caret; up and down leave it.
	if (!editing || (*editing && input.x) || !input.held) {
		g_gesture = false;
		g_selected = focus.widget;
		NativeSkillNavigation::HoldCursor(false);
		return true;
	}
	if (input.pressed && !g_gesture) {
		const auto kind = NativeUi::Classify(focus.widget).kind;
		// Let dropdowns handle their own horizontal value changes.
		if (input.x && kind == UiText::Kind::ValueControl) return true;
		const NativeMemory::ReadScope scope;
		const auto nodes = Nodes(NativeUi::Snapshot(focus.panel, focus.widget));
		const auto next = MenuNavigationPolicy::Move(nodes, g_selected, input.x, input.y);
		if (next) {
			NativeSkillNavigation::HoldCursor(true);
			if (NativeSkillNavigation::Select({focus.panel, next})) {
				g_selected = next;
				g_gesture = true;
				AsyncLog::InfoF(context, "[menu-navigation] Selected %s", NativeWidgets::ReadName(next).c_str());
			} else {
				AsyncLog::Warn(context, "[menu-navigation] Native selection unavailable; cursor navigation retained.");
			}
		}
	}
	NativeSkillNavigation::HoldCursor(g_gesture);
	return true;
}
void NoteAction(std::string_view target, std::string_view command) {
	if (!g_panel || !g_selected || target != "D2RLoaderMultiplayerPanelMessage") return;
	g_restore = 0;
	const auto name = NativeWidgets::ReadName(g_selected);
	// The command/control pairs come from the loader's live message trace.
	if (name.ends_with("Toggle") && command == "Toggle" + name.substr(0, name.size() - 6))
		g_restore = g_selected;
	else if (name.starts_with("Difficulty") && command == "Set" + name)
		g_restore = g_selected;
	else for (const std::string action : {"Increment", "Decrement"}) {
		const auto suffix = action + "Button";
		if (name.ends_with(suffix) && command == action + name.substr(0, name.size() - suffix.size()))
			g_restore = g_selected;
	}
}
}
