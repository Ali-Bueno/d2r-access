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
#include <xinput.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace D2RAccess::MenuNavigation {
namespace {
bool g_enabled = true;
// Whose a D-pad or left-stick press is until it is released: the list's moves, or the game's.
enum class Press { None, List, Game };
Press g_press = Press::None;
uintptr_t g_panel = 0, g_selected = 0, g_restore = 0;
int g_stickX = 0, g_stickY = 0;
// XInput's recommended left-stick dead zone: past it the stock cursor travels, so the list holds it.
constexpr float StickDriftFraction = float(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / std::numeric_limits<SHORT>::max();
// Diagnosis of the boxes the D-pad does not reach (the user, 2026-10-17): the form's
// text boxes and why each is or is not a D-pad stop, and whose box is being edited.
constexpr uint64_t InventoryEveryMs = 500;
// Character creation, whose name box the right stick types into as it does the loader's boxes
// (the user, 2026-11-02: "the right stick's keyboard for the character's name too").
constexpr std::string_view CharacterCreatePanel = "CharacterCreatePanel";
uint64_t g_inventoryAt = 0;
std::string g_inventory;
uintptr_t g_editor = 0;

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

// The press moving through the list: the D-pad's, a diagonal counting as up or down, else the
// left stick's dominant axis once pushed past StickPushFraction.
InputDirection::DpadInput ListInput() {
	auto pad = InputDirection::Dpad();
	if (pad.held && !pad.x && !pad.y) pad.y = pad.diagonalY;
	const auto stick = InputDirection::LeftStick();
	const float across = std::abs(stick.x), along = std::abs(stick.y);
	int x = 0, y = 0;
	if (along >= across && along >= InputDirection::StickPushFraction) y = stick.y > 0 ? 1 : -1;
	else if (across > along && across >= InputDirection::StickPushFraction) x = stick.x > 0 ? 1 : -1;
	const bool fresh = (x || y) && (x != g_stickX || y != g_stickY);
	g_stickX = x;
	g_stickY = y;
	if (pad.held) return pad;
	return {x, y, fresh, std::max(across, along) > StickDriftFraction};
}
}

namespace {
void LogBoxes(const D2RL::PluginContext* context, NativeUi::Focus focus, uint64_t now) {
	uintptr_t owner = 0;
	if (const uintptr_t editor = NativeSkillNavigation::AnyActiveTextInput(owner); editor != g_editor) {
		g_editor = editor;
		AsyncLog::InfoF(context, "[menu-navigation] editing %s (focus %s)", editor ? NativeWidgets::ReadName(editor).c_str() : "nothing",
			NativeWidgets::ReadName(focus.widget).c_str());
	}
	if (now - g_inventoryAt < InventoryEveryMs) return;
	g_inventoryAt = now;
	const NativeMemory::ReadScope scope;
	const auto snapshot = NativeUi::Snapshot(focus.panel, focus.widget);
	const auto reachable = Nodes(snapshot);
	std::string text;
	for (const auto& node : snapshot.nodes) {
		if (node.kind != UiText::Kind::TextInput) continue;
		const bool stop = std::any_of(reachable.begin(), reachable.end(), [&](const auto& n) { return n.widget == node.id; });
		text += " " + node.name + (stop ? "" : !node.enabled ? "(disabled)" : !NativeSkillNavigation::FocusRect(node.id) ? "(no rect)" : "(outside the modal)");
	}
	char counts[64]{};
	std::snprintf(counts, sizeof counts, " | %zu stops of %zu nodes, list:", reachable.size(), snapshot.nodes.size());
	text += counts;
	for (const auto& stop : reachable)
		for (const auto& node : snapshot.nodes) if (node.id == stop.widget) text += " " + node.name;
	if (text == g_inventory) return;
	g_inventory = text;
	AsyncLog::InfoF(context, "[menu-navigation] boxes:%s", text.c_str());
}
}

void Reset() {
	if (g_panel) NativeSkillNavigation::HoldCursor(false);
	PadTyping::Reset();
	g_panel = g_selected = g_restore = g_editor = 0;
	g_stickX = g_stickY = 0;
	g_inventory.clear();
	g_press = Press::None;
}
void Initialize(const D2RL::PluginContext* context) {
	Reset();
	g_enabled = RadarConfig::ReadEnabled(context, "[menu_navigation]").value_or(true);
}
// The character's name box typed with the right stick, as the loader's boxes are: while the game's
// text entry panel holds the focus for it (true: the D-pad is the game's), or while the box itself
// has the focus (the D-pad stays the game's own navigation). False when neither.
static std::optional<bool> TypeName(const D2RL::PluginContext* context, const NativeUi::Focus& focus, uint64_t now) {
	uintptr_t owner = 0;
	const uintptr_t editor = NativeSkillNavigation::AnyActiveTextInput(owner);
	if (editor && owner && focus.panel != owner && NativeWidgets::ReadName(owner) == CharacterCreatePanel) {
		PadTyping::Tick(context, {owner, editor}, true, now);
		return true;
	}
	if (!focus.widget || !focus.panel || NativeWidgets::ReadName(focus.panel) != CharacterCreatePanel
		|| NativeUi::Classify(focus.widget).kind != UiText::Kind::TextInput) return std::nullopt;
	auto editing = NativeSkillNavigation::TextEditing();
	if (editing && *editing) editing = NativeSkillNavigation::ActiveTextInput(focus.panel) == focus.widget;
	PadTyping::Tick(context, focus, editing, now);
	return false;
}

bool Tick(const D2RL::PluginContext* context) {
	auto focus = NativeUi::CurrentFocus();
	if (g_enabled) {
		if (const auto typed = TypeName(context, focus, GetTickCount64())) {
			if (g_panel) Reset();
			return *typed;
		}
	}
	// While a lobby box is edited, the game's text entry panel holds the focus: the
	// right stick still types into the box, and the D-pad is the game's.
	uintptr_t owner = 0;
	if (const uintptr_t editor = NativeSkillNavigation::AnyActiveTextInput(owner); g_enabled && editor && owner == g_panel
		&& g_panel && focus.panel != g_panel) {
		LogBoxes(context, {owner, editor}, GetTickCount64());
		PadTyping::Tick(context, {owner, editor}, true, GetTickCount64());
		NativeSkillNavigation::HoldCursor(false);
		return true;
	}
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
	LogBoxes(context, focus, GetTickCount64());
	auto editing = NativeSkillNavigation::TextEditing();
	if (editing && *editing) editing = NativeSkillNavigation::ActiveTextInput(focus.panel) == focus.widget;
	PadTyping::Tick(context, focus, editing, GetTickCount64());
	const auto input = ListInput();
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
		g_press = Press::None;
		g_selected = focus.widget;
		NativeSkillNavigation::HoldCursor(false);
		return true;
	}
	// Let dropdowns handle their own horizontal value changes.
	if (input.pressed && g_press == Press::None && input.x
		&& NativeUi::Classify(focus.widget).kind == UiText::Kind::ValueControl) g_press = Press::Game;
	if (input.pressed && g_press == Press::None && (input.x || input.y)) {
		g_press = Press::List;
		const NativeMemory::ReadScope scope;
		const auto nodes = Nodes(NativeUi::Snapshot(focus.panel, focus.widget));
		// Up and down walk the form in its own order, one control a press; left and right stay
		// within a row of controls, and nothing moves past its ends.
		const auto next = input.y ? MenuNavigationPolicy::Step(nodes, g_selected, input.y)
			: MenuNavigationPolicy::Beside(nodes, g_selected, input.x);
		if (!next) g_press = Press::Game;
		else if (next != g_selected) {
			NativeSkillNavigation::HoldCursor(true);
			if (NativeSkillNavigation::Select({focus.panel, next})) {
				g_selected = next;
				AsyncLog::InfoF(context, "[menu-navigation] Selected %s", NativeWidgets::ReadName(next).c_str());
			} else {
				g_press = Press::Game;
				AsyncLog::Warn(context, "[menu-navigation] Native selection unavailable; cursor navigation retained.");
			}
		}
	}
	// Even a press that moved nothing, or a stick not yet pushed far enough, keeps the stock
	// cursor still: it would travel off the list.
	NativeSkillNavigation::HoldCursor(g_press != Press::Game);
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
