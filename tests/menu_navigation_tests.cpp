#include "menu_navigation.h"
#include "native_memory.h"
#include "input_direction.h"
#include "native_skill_navigation.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "radar_config.h"
#include "skill_navigation.h"
#include "test_support.h"
#include "async_log.h"

#include <map>
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace D2RAccess;
NativeUi::Focus focus{1, 10};
UiText::Snapshot snapshot;
InputDirection::DpadInput dpad;
std::map<uintptr_t, NativeUi::Rect> rectangles;
std::optional<bool> editing = false;
bool available = true, selectionWorks = true, cursorHeld = false;
int selections = 0, skillResets = 0;

UiText::Node Node(uintptr_t id, int parent, UiText::Kind kind, std::string name,
	bool enabled = true) {
	UiText::Node node;
	node.id = id;
	node.parent = parent;
	node.kind = kind;
	node.name = std::move(name);
	node.enabled = enabled;
	return node;
}

void Release() {
	dpad = {};
	MenuNavigation::Tick(nullptr);
}
void Press(int dx, int dy) {
	dpad = {dx, dy, true, true};
	MenuNavigation::Tick(nullptr);
}
}

namespace D2RAccess::InputDirection { DpadInput Dpad() { return dpad; } }
namespace D2RAccess::PadTyping {
void Tick(const D2RL::PluginContext*, NativeUi::Focus, std::optional<bool>, uint64_t) {}
void Reset() {}
}
namespace D2RAccess::NativeSkillNavigation {
bool Available() { return available; }
void HoldCursor(bool hold) { cursorHeld = hold; }
std::optional<NativeUi::Rect> FocusRect(uintptr_t widget) {
	const auto it = rectangles.find(widget);
	if (it == rectangles.end()) return std::nullopt;
	return it->second;
}
std::optional<bool> TextEditing() { return editing; }
bool Select(NativeUi::Focus next) {
	++selections;
	if (selectionWorks) focus = next;
	return selectionWorks;
}
}
namespace D2RAccess::NativeUi {
Focus CurrentFocus() { return focus; }
Type Classify(uintptr_t widget) {
	for (const auto& node : snapshot.nodes) if (node.id == widget) return {"", node.kind, UiText::Surface::Other};
	return {};
}
UiText::Snapshot Snapshot(uintptr_t, uintptr_t) { return snapshot; }
}
namespace D2RAccess::NativeWidgets {
std::string ReadName(uintptr_t widget) {
	if (widget == 1) return "D2RLoaderMultiplayerPanel";
	for (const auto& node : snapshot.nodes) if (node.id == widget) return node.name;
	return "OtherPanel";
}
}
namespace D2RAccess::NativeMemory {
ReadScope::ReadScope() noexcept = default;
ReadScope::~ReadScope() = default;
}
namespace D2RAccess::RadarConfig {
std::optional<bool> ReadEnabled(const D2RL::PluginContext*, std::string_view) { return true; }
}
namespace D2RAccess::SkillNavigation { void Reset() { ++skillResets; } }
namespace D2RAccess::AsyncLog {
void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {}
void Warn(const D2RL::PluginContext*, const char*) noexcept {}
}

int main() {
	using namespace D2RAccess;
	snapshot.panel = 1;
	snapshot.nodes = {
		Node(1, -1, UiText::Kind::Other, "D2RLoaderMultiplayerPanel"),
		Node(10, 0, UiText::Kind::TextInput, "HostGameNameInput"),
		Node(11, 0, UiText::Kind::Toggle, "DisabledToggle", false),
		Node(12, 0, UiText::Kind::TextInput, "HostPasswordInput"),
		Node(13, 0, UiText::Kind::Button, "HostConfirmButton"),
	};
	rectangles = {{10, {20, 20, 280, 30}}, {11, {20, 65, 120, 30}},
		{12, {20, 110, 280, 30}}, {13, {20, 155, 120, 30}}};
	MenuNavigation::Initialize(nullptr);
	Check(MenuNavigation::Tick(nullptr) && skillResets == 1, "multiplayer panel takes navigation ownership");
	Press(0, 1);
	Check(focus.widget == 12 && selections == 1 && cursorHeld,
		"one short press selects the next enabled control and holds stock cursor travel");
	dpad.pressed = false;
	for (int i = 0; i < 20; ++i) MenuNavigation::Tick(nullptr);
	Check(focus.widget == 12 && selections == 1, "holding a direction does not repeat selection");
	Release();
	Check(!cursorHeld, "full release restores stock cursor travel");
	Press(0, 1);
	Check(focus.widget == 13 && selections == 2, "new press after release advances once");
	Release();
	focus.widget = 10;
	editing = true;
	Press(0, 1);
	Check(focus.widget == 10 && selections == 2 && !cursorHeld,
		"native text editing leaves D-pad control with the input field");
	Release();
	editing = std::nullopt;
	Press(0, 1);
	Check(focus.widget == 10 && selections == 2 && !cursorHeld,
		"unknown native editing state also preserves stock behavior");
	Release();
	editing = false;
	snapshot.nodes.push_back(Node(20, 0, UiText::Kind::Other, "DirectJoinModal"));
	snapshot.nodes.push_back(Node(21, 5, UiText::Kind::TextInput, "DirectJoinAddressInput"));
	snapshot.nodes.push_back(Node(22, 5, UiText::Kind::Button, "DirectJoinConfirmButton"));
	rectangles[21] = {40, 60, 240, 30};
	rectangles[22] = {40, 110, 140, 30};
	Press(0, 1);
	Check(focus.widget == 21 && selections == 3, "visible modal confines navigation to its descendants");
	Release();
	Press(0, 1);
	Check(focus.widget == 22 && selections == 4, "modal controls follow their own screen order");
	Release();
	focus = {2, 99};
	Check(!MenuNavigation::Tick(nullptr) && !cursorHeld && skillResets == 1,
		"leaving panel releases cursor and hands routing back without resetting skills");

	snapshot.nodes = {
		Node(1, -1, UiText::Kind::Other, "D2RLoaderMultiplayerPanel"),
		Node(10, 0, UiText::Kind::TextInput, "HostGameNameInput"),
		Node(30, 0, UiText::Kind::Toggle, "UnlistedToggle"),
		Node(31, 0, UiText::Kind::Button, "HostConfirmButton"),
	};
	rectangles[30] = {20, 70, 120, 30};
	rectangles[31] = {20, 120, 120, 30};
	focus = {1, 10};
	MenuNavigation::Tick(nullptr);
	Press(0, 1);
	Check(focus.widget == 30, "D-pad can reach the toggle before its value action");
	Release();
	snapshot.nodes[2].toggled = true; // the loader has already applied the user's A press
	MenuNavigation::NoteAction("D2RLoaderMultiplayerPanelMessage", "ToggleUnlisted");
	focus.widget = 10; // loader rebuild reset native focus to the first text field
	const int beforeRestore = selections;
	MenuNavigation::Tick(nullptr);
	Check(focus.widget == 30 && selections == beforeRestore + 1,
		"matching value command restores the toggle once after loader focus reset");
	Check(snapshot.nodes[2].toggled == true, "focus restoration preserves the loader's new toggle value");
	focus.widget = 10;
	MenuNavigation::Tick(nullptr);
	Check(focus.widget == 10 && selections == beforeRestore + 1,
		"restoration is consumed and does not fight later native focus changes");

	const auto armToggle = [] {
		focus.widget = 30;
		MenuNavigation::Tick(nullptr);
	};
	armToggle();
	MenuNavigation::NoteAction("OtherPanelMessage", "ToggleUnlisted");
	focus.widget = 10;
	MenuNavigation::Tick(nullptr);
	Check(focus.widget == 10, "unrelated message target cannot restore focus");
	armToggle();
	MenuNavigation::NoteAction("D2RLoaderMultiplayerPanelMessage", "SelectTab");
	focus.widget = 10;
	MenuNavigation::Tick(nullptr);
	Check(focus.widget == 10, "tab commands cannot restore stale value-control focus");
	armToggle();
	MenuNavigation::NoteAction("D2RLoaderMultiplayerPanelMessage", "ToggleUnlisted");
	snapshot.nodes.push_back(Node(40, 0, UiText::Kind::Other, "DirectJoinModal"));
	snapshot.nodes.push_back(Node(41, 4, UiText::Kind::TextInput, "DirectJoinAddressInput"));
	rectangles[41] = {30, 50, 200, 30};
	focus.widget = 10;
	MenuNavigation::Tick(nullptr);
	Check(focus.widget == 10, "opened modal excludes a background toggle from restoration");
	snapshot.nodes.resize(4);

	armToggle();
	MenuNavigation::NoteAction("D2RLoaderMultiplayerPanelMessage", "ToggleUnlisted");
	focus.widget = 10;
	editing = true;
	const int beforeEdit = selections;
	MenuNavigation::Tick(nullptr);
	Check(focus.widget == 10 && selections == beforeEdit,
		"active native text edit takes precedence over pending restoration");
	editing = false;
	MenuNavigation::Tick(nullptr);
	Check(focus.widget == 10, "skipped restoration does not replay after editing ends");

	armToggle();
	MenuNavigation::NoteAction("D2RLoaderMultiplayerPanelMessage", "ToggleUnlisted");
	focus.widget = 10;
	Press(0, 1);
	Check(focus.widget == 31, "new D-pad press takes precedence over pending focus restoration");
	Release();
	Passed();
}
