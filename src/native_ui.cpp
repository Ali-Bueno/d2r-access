#include "native_ui.h"
#include "localization.h"
#include "native_memory.h"
#include "native_widgets.h"
#include "quest_log.h"
#include "tomb_symbols.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <array>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace D2RAccess::NativeUi {
namespace {
using NativeMemory::Peek;
using UiText::Kind;
using UiText::Surface;
// All offsets are from the verified live D2R 3.3.93787 functions, not SDK handles.
// Full evidence and call sites: docs/native-ui.md.
constexpr size_t ParentOffset = 0x30;
constexpr size_t EnabledOffset = 0x50;
constexpr size_t VisibleOffset = 0x51;
constexpr size_t RectOffset = 0x70; // widget rect reflection record; docs/native-ui.md
constexpr size_t ChildrenOffset = 0x58; // FindChildWidget 0x855C90
constexpr size_t GetTypeSlot = 0x58;    // vtable, distinct from the instance member
constexpr size_t TypeNameOffset = 0x8, BaseTypeOffset = 0x10; // 0x851030
constexpr size_t ButtonTextOffset = 0x818; // 0x8496CA calls SetText on embedded TextBoxWidget
constexpr size_t ToggleStateOffset = 0xB88; // isToggled reflection and RVA 0x882D40 getter, D2R 3.3.93787
constexpr size_t ToggleGetterSlot = 0x8;
constexpr size_t ToggleGetterStateInstruction = 6;
constexpr std::array<uint8_t, 7> ToggleGetterBytes { 0x0F, 0xB6, 0x91, 0x88, 0x0B, 0x00, 0x00 };
constexpr size_t InputTextOffset = 0x558; // InputTextBoxWidget getter RVA 0x889EC0, D2R 3.3.93787
constexpr size_t InputTextGetterSlot = 0x70;
constexpr size_t InputTextGetterInstruction = 6;
constexpr std::array<uint8_t, 7> InputTextGetterBytes { 0x48, 0x8B, 0x81, 0x58, 0x05, 0x00, 0x00 };
constexpr size_t ButtonLegendOffset = 0x310; // FocusableWidget registration 0x875CB0; live gold_button
constexpr size_t EquipmentLocationOffset = 0x5D8; // InventorySlotWidget location, live metadata 0x236C530
constexpr size_t LinkedTextOffset = 0x570; // 0x87DC10 / 0x87EFE0 delegate to embedded TextBoxWidget
constexpr size_t ChatDataSourceOffset = 0x300; // ChatListWidget::Count, 0x84CC70
constexpr uint32_t ChatNotificationMessages = 1; // ChatDataSource registration, 0x8BA3A0
constexpr size_t SelectedTabOffset = 0x16C4; // TabBarWidget input handler, 0x878D30
constexpr size_t TabCountOffset = 0x558, TabEnabledOffset = 0x1788; // 0x878CA0/0x877D20; VendorPanel 0x2420C0
constexpr int MaxTabs = 8; // TabBarWidget's enabled-flag array
constexpr size_t SelectionOffset = 0x544; // FocusableWidget x/y, InventoryItemWidget 0x2A89C0
constexpr size_t StashCountTextOffset = 0x618; // AdvancedStashSlotWidget countText (TextBoxWidget), field table 0x2CE9B0
constexpr size_t GridDimensionsOffset = 0x628; // InventoryGridWidget update, 0x2C5FA0
constexpr size_t ItemAtCellSlot = 0xC8; // 0x2A89C0 calls this virtual getter with the selected x/y
constexpr size_t FocusManagerOffset = 0xD0; // 0x846020
constexpr size_t FocusPanelOffset = 0x188, FocusWidgetOffset = 0x190; // 0x868230 / 0x876CA0
constexpr size_t PanelVectors[] = { 0x58, 0x88, 0xA0 }; // root children + FindPanel 0x89F760
// Bounds protect traversal from stale/corrupt pointers and cap UI-thread work.
constexpr size_t MaxNodes = 512, MaxDepth = 32, MaxTypeName = 96;
std::unordered_map<uintptr_t, Type> g_types;
// Read from the tooltip draw as well as from the UI update.
std::atomic<uintptr_t> g_cursorPanel{0}, g_cursorWidget{0};
std::atomic<uint64_t> g_cursorSelection{UINT64_MAX};

uintptr_t CallType(uintptr_t function, uintptr_t widget) noexcept {
	__try {
		return reinterpret_cast<uintptr_t(*)(uintptr_t)>(function)(widget);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

ItemPresence CallItemAtCell(uintptr_t function, uintptr_t widget, Cell cell) noexcept {
	__try {
		const auto item = reinterpret_cast<uintptr_t(*)(uintptr_t, const Cell*)>(function)(widget, &cell);
		return item ? ItemPresence::Occupied : ItemPresence::Empty;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return ItemPresence::Unknown; }
}

bool CallItemPointer(uintptr_t function, uintptr_t widget, Cell cell, uintptr_t& item) noexcept {
	__try {
		item = reinterpret_cast<uintptr_t(*)(uintptr_t, const Cell*)>(function)(widget, &cell);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

std::vector<uintptr_t> VectorAt(uintptr_t address) {
	struct { uintptr_t data; uint64_t count; } vector{};
	if (!Peek(address, vector) || vector.count > MaxNodes || (vector.count && !vector.data)) return {};
	std::vector<uintptr_t> items(static_cast<size_t>(vector.count));
	if (!items.empty() && !NativeMemory::Copy(vector.data, items.data(), items.size() * sizeof(uintptr_t))) return {};
	return items;
}

std::optional<bool> ToggleState(uintptr_t widget) {
	uintptr_t vtable = 0, getter = 0;
	std::array<uint8_t, ToggleGetterBytes.size()> instruction{};
	uint8_t state = 0;
	if (!Peek(widget, vtable) || !Peek(vtable + ToggleGetterSlot, getter)
		|| !NativeMemory::Executable(getter)
		|| !NativeMemory::Copy(getter + ToggleGetterStateInstruction, instruction.data(), instruction.size())
		|| instruction != ToggleGetterBytes || !Peek(widget + ToggleStateOffset, state) || state > 1) return std::nullopt;
	return state != 0;
}

}

std::optional<bool> Enabled(uintptr_t widget) {
	uint8_t enabled = 0;
	if (!widget || !Peek(widget + EnabledOffset, enabled) || enabled > 1) return std::nullopt;
	return enabled != 0;
}

std::optional<bool> Toggled(uintptr_t widget) {
	return widget && Classify(widget).name == "ToggleButtonWidget" ? ToggleState(widget) : std::nullopt;
}

namespace {
// Set to 1 by RVA 0x86EEC0 and to 0 by 0x86EFB0, read by the click handler (native_buttons.cpp).
constexpr size_t ButtonDisabledOffset = 0x67A;
std::atomic<bool> g_buttonsVerified{false};
}

void SetButtonsVerified(bool verified) { g_buttonsVerified = verified; }

std::optional<bool> ButtonDisabled(uintptr_t widget) {
	if (!g_buttonsVerified || !widget || Classify(widget).name != "ButtonWidget") return std::nullopt;
	uint8_t disabled = 0;
	if (!Peek(widget + ButtonDisabledOffset, disabled) || disabled > 1) return std::nullopt;
	return disabled != 0;
}

namespace {
void MatchType(Type& result, const std::string& name) {
	// Classify inheritance, never a runtime panel's name. A future subtype uses
	// the same archetype automatically; unknown widgets still expose child text.
	if (name == "NpcDialogPanel" || name == "QuestLogPanel") result.surface = Surface::Menu;
	else if (name == "ScrollSpeechPanel" || name == "QuestLogSpeechPanel") result.surface = Surface::Narrative;
	else if (name == "TutorialTooltip" || name == "DismissibleTooltipPanel") result.surface = Surface::Tip;
	else if (name == "LevelNamePanel") result.surface = Surface::Area;
	else if (name == "NotificationPanel") result.surface = Surface::Notice;
	else if (name == "HUDMessagePanel") result.surface = Surface::NotificationFeed;
	else if (name == "MonsterHealthPanel") result.surface = Surface::Target;
	if (result.kind != Kind::Other) return;
	if (name == "TextBoxWidget") result.kind = Kind::Text;
	else if (name == "InputTextBoxWidget" || name == "MaskedInputTextBoxWidget"
		|| name == "NumericInputTextBoxWidget") result.kind = Kind::TextInput;
	else if (name == "TextBoxWithLinksWidget") result.kind = Kind::LinkedText;
	else if (name == "ChatListWidget") result.kind = Kind::NotificationList;
	else if (name == "ToggleButtonWidget") result.kind = Kind::Toggle; // existing value reader retains toggles
	else if (name == "DropdownListWidget" || name == "CarouselWidget" || name == "SliderWidget"
		|| name == "SettingsSliderWidget") result.kind = Kind::ValueControl;
	else if (name == "CharacterStatLevelUpButtonWidget") result.kind = Kind::StatButton;
	else if (name == "ButtonLegendWidget") result.kind = Kind::Legend;
	else if (name == "TabWidget") result.kind = Kind::TabButton;
	else if (name == "ButtonWidget") result.kind = Kind::Button;
	else if (name == "ListItemWidget" || name == "HoverListItemWidget") result.kind = Kind::ListItem;
	else if (name == "ListWidget") result.kind = Kind::List;
	else if (name == "InventoryGridWidget") result.kind = Kind::InventoryGrid;
	else if (name == "InventorySlotWidget") result.kind = Kind::InventorySlot;
	else if (name == "InventoryItemWidget") result.kind = Kind::InventoryItem;
	else if (name == "TabBarWidget") result.kind = Kind::Tabs;
	else if (name == "FocusableWidget") result.kind = Kind::Focusable;
	else if (name == "ScrollingSpeechWidget") result.kind = Kind::ScrollingText;
	else if (name == "QuestLogButtonWidget") result.kind = Kind::QuestButton;
}

void Walk(uintptr_t widget, int parent, size_t depth, UiText::Snapshot& snapshot, std::unordered_set<uintptr_t>& visited) {
	if (!widget || depth > MaxDepth || snapshot.nodes.size() >= MaxNodes || !visited.insert(widget).second) return;
	uint8_t visible = 0, enabled = 0;
	if (!Peek(widget + VisibleOffset, visible) || visible != 1 || !Peek(widget + EnabledOffset, enabled) || enabled > 1) return;
	UiText::Node node;
	node.id = widget;
	node.parent = parent;
	node.enabled = enabled != 0 && (parent < 0 || snapshot.nodes[static_cast<size_t>(parent)].enabled);
	node.name = NativeWidgets::ReadName(widget);
	// This is a template container in the layouts, not a visible menu entry.
	// Clones may retain "ItemTemplate" as a name, so never filter on suffixes.
	if (node.name == "Templates") return;
	const Type type = Classify(widget);
	node.type = type.name;
	node.kind = type.kind;
	if (type.name == "ToggleButtonWidget") node.toggled = ToggleState(widget);
	if (type.kind == Kind::Text) node.text = NativeWidgets::ReadText(widget);
	else if (type.kind == Kind::TextInput) node.text = InputText(widget);
	else if (type.kind == Kind::LinkedText) node.text = NativeWidgets::ReadText(widget + LinkedTextOffset);
	else if (type.name == "DropdownListWidget") {
		node.text = NativeWidgets::ReadText(widget + ButtonTextOffset);
		node.hint = NativeWidgets::ReadText(widget);
	}
	else if (type.kind == Kind::Button || type.kind == Kind::Toggle || type.kind == Kind::StatButton) {
		node.text = NativeWidgets::ReadText(widget + ButtonTextOffset);
		// A button's +0x88 string is FocusableWidget::tooltipString, not its
		// embedded caption. Icon buttons such as gold_button have only this text.
		const auto hint = NativeWidgets::ReadText(widget);
		if (node.text.empty()) node.text = hint;
		else if (!hint.empty() && UiText::Clean(hint) != UiText::Clean(node.text)) node.text += ". " + hint;
		if (node.text.empty()) (void)NativeMemory::ReadEngineString(widget + ButtonLegendOffset, node.text);
	}
	else if (type.kind == Kind::QuestButton) {
		// A quest button is an icon with no caption, so the reader would land on a
		// nameless control. Its own strings come first when the game fills them;
		// otherwise the quest it stands for is named from the game's string table.
		node.text = NativeWidgets::ReadText(widget);
		if (node.text.empty()) {
			const auto resolve = [](std::string_view key) { return Localization::Text(key); };
			if (const auto* quest = QuestLog::ForWidget(node.name)) node.text = QuestLog::Title(*quest, resolve);
		}
	}
	else if (type.kind == Kind::Tabs) node.text = SelectedTabText(widget);
	else if (type.kind == Kind::TabButton) node.text = TabLabel(widget);
	if (type.kind == Kind::NotificationList) {
		uint32_t source = 0;
		if (!Peek(widget + ChatDataSourceOffset, source) || source != ChatNotificationMessages) node.kind = Kind::List;
		else {
			auto message = NativeWidgets::ReadLatestNotification(widget);
			node.contentId = message.id;
			node.text = std::move(message.text);
		}
	}
	if (type.kind == Kind::ScrollingText && snapshot.surface == Surface::Other) snapshot.surface = Surface::Narrative;
	const int index = static_cast<int>(snapshot.nodes.size());
	snapshot.nodes.push_back(std::move(node));
	for (uintptr_t child : VectorAt(widget + ChildrenOffset)) Walk(child, index, depth + 1, snapshot, visited);
}
}

Type Classify(uintptr_t widget) {
	if (!NativeWidgets::SupportsTree() || widget == 0) return {};
	uintptr_t vtable = 0, function = 0;
	if (!Peek(widget, vtable) || !Peek(vtable + GetTypeSlot, function)) return {};
	if (auto it = g_types.find(vtable); it != g_types.end()) return it->second;
	if (!NativeMemory::Executable(function)) return {};
	Type result;
	uintptr_t type = CallType(function, widget);
	for (size_t depth = 0; type && depth < MaxDepth; ++depth) {
		uintptr_t name = 0, base = 0;
		if (!Peek(type + TypeNameOffset, name) || !Peek(type + BaseTypeOffset, base)) return {};
		const std::string text = NativeMemory::CString(name, MaxTypeName);
		if (text.empty()) return {};
		if (result.name.empty()) result.name = text;
		MatchType(result, text);
		if (base == type) return {};
		type = base;
	}
	if (!result.name.empty() && g_types.size() < MaxNodes) g_types.emplace(vtable, result);
	return result;
}

bool Visible(uintptr_t widget) {
	if (!NativeWidgets::SupportsTree() || !widget) return false;
	for (size_t depth = 0; widget && depth < MaxDepth; ++depth) {
		uint8_t visible = 0;
		uintptr_t parent = 0;
		if (!Peek(widget + VisibleOffset, visible) || visible != 1 || !Peek(widget + ParentOffset, parent) || parent == widget) return false;
		widget = parent;
		if (!widget) return true;
	}
	return false;
}

uintptr_t Parent(uintptr_t widget) {
	uintptr_t parent = 0;
	if (!NativeWidgets::SupportsTree() || !widget || !Peek(widget + ParentOffset, parent) || parent == widget) return 0;
	return parent;
}

std::string DropdownText(uintptr_t widget) {
	if (!NativeWidgets::SupportsTree() || !widget || Classify(widget).name != "DropdownListWidget") return {};
	return NativeWidgets::ReadText(widget + ButtonTextOffset);
}

std::string InputText(uintptr_t widget) {
	if (!NativeWidgets::SupportsTree() || !widget) return {};
	// Any text box but the masked one, the chat's autocomplete box included; the getter
	// check below refuses a subtype whose getter is not the verified one.
	const auto type = Classify(widget);
	if (type.kind != Kind::TextInput || type.name == "MaskedInputTextBoxWidget") return {};
	uintptr_t vtable = 0, getter = 0;
	std::array<uint8_t, InputTextGetterBytes.size()> instruction{};
	if (!Peek(widget, vtable) || !Peek(vtable + InputTextGetterSlot, getter)
		|| !NativeMemory::Executable(getter)
		|| !NativeMemory::Copy(getter + InputTextGetterInstruction, instruction.data(), instruction.size())
		|| instruction != InputTextGetterBytes) return {};
	std::string text;
	return NativeMemory::ReadEngineString(widget + InputTextOffset, text) ? text : std::string{};
}

std::optional<Rect> LocalRect(uintptr_t widget) {
	Rect result;
	if (!NativeWidgets::SupportsTree() || !widget || !Peek(widget + RectOffset, result)
		|| result.width < 0 || result.height < 0) return std::nullopt;
	return result;
}

std::vector<uintptr_t> Children(uintptr_t widget) {
	return NativeWidgets::SupportsTree() && widget ? VectorAt(widget + ChildrenOffset) : std::vector<uintptr_t>{};
}

void SetCursor(Focus focus, uint64_t selection) {
	g_cursorSelection = selection;
	g_cursorPanel = focus.panel;
	g_cursorWidget = focus.widget;
}

void ClearCursor() {
	g_cursorWidget = 0;
	g_cursorPanel = 0;
	g_cursorSelection = UINT64_MAX;
}

Focus CurrentFocus() {
	if (const Focus cursor{g_cursorPanel.load(), g_cursorWidget.load()};
		cursor.panel && cursor.widget && Visible(cursor.panel) && Visible(cursor.widget)) return cursor;
	return GameFocus();
}

Focus GameFocus() {
	if (!NativeWidgets::SupportsTree()) return {};
	uintptr_t manager = 0;
	Focus result;
	const uintptr_t panels = NativeWidgets::PanelManager();
	if (!panels || !Peek(panels + FocusManagerOffset, manager) || !manager
		|| !Peek(manager + FocusPanelOffset, result.panel) || !Peek(manager + FocusWidgetOffset, result.widget)) return {};
	if (!Visible(result.panel)) return {};
	if (!Visible(result.widget)) result.widget = 0;
	// A panel drawing nothing cannot hold focus: EmoteWheelPanel stays open all
	// game and only hides its Container, and kept world speech muted. docs/native-ui.md
	if (!result.widget && DrawsNothing(result.panel)) return {};
	return result;
}

bool DrawsNothing(uintptr_t panel) {
	if (!panel) return false; // not found: nothing known against it
	const auto children = Children(panel);
	return std::none_of(children.begin(), children.end(), [](uintptr_t child) { return Visible(child); });
}

uint64_t FocusSelection(uintptr_t widget) {
	if (widget && widget == g_cursorWidget.load()) return g_cursorSelection.load();
	uint64_t result = UINT64_MAX;
	if (NativeWidgets::SupportsTree() && widget) (void)Peek(widget + SelectionOffset, result);
	return result;
}

std::string SelectedTabText(uintptr_t widget) {
	if (!Visible(widget) || Classify(widget).kind != Kind::Tabs) return {};
	int32_t selected = -1;
	if (!Peek(widget + SelectedTabOffset, selected) || selected < 0 || selected >= 8) return {};
	return NativeWidgets::ReadText(NativeWidgets::FindChild(widget, "Text" + std::to_string(selected)));
}

int SelectedTab(uintptr_t widget) {
	int32_t selected = -1;
	if (!Visible(widget) || Classify(widget).kind != Kind::Tabs
		|| !Peek(widget + SelectedTabOffset, selected) || selected < 0 || selected >= 8) return -1;
	return selected;
}

int TabCount(uintptr_t widget) {
	if (!Visible(widget) || Classify(widget).kind != Kind::Tabs) return 0;
	int count = 0;
	// A bar names its tabs Text0 onward; one it does not show has no caption.
	while (count < 8 && !NativeWidgets::ReadText(NativeWidgets::FindChild(widget, "Text" + std::to_string(count))).empty()) ++count;
	return count;
}

int AdjacentTab(uintptr_t widget, int delta) {
	if ((delta != -1 && delta != 1) || !Visible(widget) || Classify(widget).kind != Kind::Tabs) return -1;
	int32_t count = 0;
	std::array<uint8_t, MaxTabs> enabled{};
	if (!Peek(widget + TabCountOffset, count) || count < 1 || count > MaxTabs
		|| !NativeMemory::Copy(widget + TabEnabledOffset, enabled.data(), count)) return -1;
	for (int index = 0; index < count; ++index) if (enabled[index] > 1) return -1;
	const int open = SelectedTab(widget);
	// The shop keeps every caption but disables empty categories; skip them as the native hit test does.
	for (int next = open >= 0 && open < count ? open + delta : delta > 0 ? 0 : count - 1;
		next >= 0 && next < count; next += delta)
		if (enabled[next]) return next;
	return open >= 0 && open < count && enabled[open] ? open : -1;
}

std::string TabLabel(uintptr_t widget) {
	const std::string name = NativeWidgets::ReadName(widget);
	bool numbered = name.size() > 3 && name.starts_with("Tab");
	for (size_t i = 3; numbered && i < name.size(); ++i) numbered = name[i] >= '0' && name[i] <= '9';
	if (numbered) {
		const std::string label = "Text" + name;
		uintptr_t parent = Parent(widget);
		for (int depth = 0; parent && depth < 4; ++depth, parent = Parent(parent))
			if (const uintptr_t text = NativeWidgets::FindChild(parent, label); text && Visible(text)) {
				if (auto read = NativeWidgets::ReadText(text); !read.empty()) return read;
			}
	}
	return NativeWidgets::ReadText(widget);
}

std::string FocusedTabText(uintptr_t widget) {
	if (!Visible(widget) || Classify(widget).kind != Kind::Tabs) return {};
	const auto x = static_cast<int32_t>(FocusSelection(widget));
	if (x < 0 || x >= 8) return {};
	return NativeWidgets::ReadText(NativeWidgets::FindChild(widget, "Text" + std::to_string(x)));
}

Cell GridCell(uintptr_t widget) {
	Cell cell;
	if (Classify(widget).kind != Kind::InventoryGrid) return cell;
	struct { int32_t width, height; } dimensions{};
	const uint64_t selection = FocusSelection(widget);
	const auto x = static_cast<int32_t>(selection), y = static_cast<int32_t>(selection >> 32);
	if (!Peek(widget + GridDimensionsOffset, dimensions) || dimensions.width < 1 || dimensions.height < 1
		|| dimensions.width > 64 || dimensions.height > 64 || x < 0 || y < 0
		|| x >= dimensions.width || y >= dimensions.height) return cell;
	return {x, y};
}

int EquipmentLocation(uintptr_t widget) {
	int32_t location = 0;
	if (Classify(widget).kind != Kind::InventorySlot || !Visible(widget)
		|| !Peek(widget + EquipmentLocationOffset, location)) return 0;
	return location; // caller maps only the verified body locations
}

ItemPresence GridItemPresence(uintptr_t widget) {
	if (!NativeWidgets::SupportsTree() || !Visible(widget)) return ItemPresence::Unknown;
	const auto cell = GridCell(widget);
	uintptr_t vtable = 0, getter = 0;
	if (cell.x < 0 || !Peek(widget, vtable) || !vtable || !Peek(vtable + ItemAtCellSlot, getter)
		|| !NativeMemory::Executable(getter)) return ItemPresence::Unknown;
	// InventoryGridWidget 0x2C49F0 queries the widget's owner/page and cell;
	// its 0x386A00 lookup reads the inventory grid, without moving any item.
	return CallItemAtCell(getter, widget, cell);
}

uintptr_t GridItem(uintptr_t widget) {
	return GridItemAt(widget, GridCell(widget)).item;
}

Cell GridSize(uintptr_t widget) {
	struct { int32_t width, height; } dimensions{};
	if (Classify(widget).kind != Kind::InventoryGrid || !Peek(widget + GridDimensionsOffset, dimensions)
		|| dimensions.width < 1 || dimensions.height < 1 || dimensions.width > 64 || dimensions.height > 64) return {};
	return {dimensions.width, dimensions.height};
}

GridSlot GridItemAt(uintptr_t widget, Cell cell) {
	GridSlot result;
	if (!NativeWidgets::SupportsTree() || !Visible(widget)) return result;
	const auto size = GridSize(widget);
	uintptr_t vtable = 0, getter = 0;
	if (cell.x < 0 || cell.y < 0 || cell.x >= size.x || cell.y >= size.y || !Peek(widget, vtable) || !vtable
		|| !Peek(vtable + ItemAtCellSlot, getter) || !NativeMemory::Executable(getter)) return result;
	result.known = CallItemPointer(getter, widget, cell, result.item);
	return result;
}

StashSlotCount StashSlotStackCount(uintptr_t widget) {
	if (!widget || !Visible(widget) || Classify(widget).name != "AdvancedStashSlotWidget") return {};
	const uintptr_t count = widget + StashCountTextOffset;
	return {NativeWidgets::ReadText(count), Visible(count), true}; // update 0x2CF990 hides it at zero
}

uintptr_t EquipmentItem(uintptr_t widget) {
	if (Classify(widget).kind != Kind::InventorySlot || !Visible(widget)) return 0;
	uintptr_t vtable = 0, getter = 0, item = 0;
	if (!Peek(widget, vtable) || !Peek(vtable + ItemAtCellSlot, getter)
		|| !NativeMemory::Executable(getter) || !CallItemPointer(getter, widget, {}, item)) return 0;
	return item;
}

ItemPresence EquipmentItemPresence(uintptr_t widget) {
	if (Classify(widget).kind != Kind::InventorySlot || !Visible(widget)) return ItemPresence::Unknown;
	uintptr_t vtable = 0, getter = 0;
	if (!Peek(widget, vtable) || !Peek(vtable + ItemAtCellSlot, getter)
		|| !NativeMemory::Executable(getter)) return ItemPresence::Unknown;
	// InventorySlotWidget vtable 0x1CF3F20 +0xC8 -> 0x2CA060: reads the
	// widget owner and location +0x5D8, then 0x3886D0 gets the equipped item.
	// Unlike the grid getter, this override ignores the cell argument.
	return CallItemAtCell(getter, widget, {});
}

namespace {
BeltSlot QueryBelt(uintptr_t widget, Cell cell) {
	BeltSlot result;
	if (cell.x < 0 || cell.x >= BeltSlots || !NativeWidgets::SupportsTree() || !Visible(widget)
		|| Classify(widget).name != "BeltWidget") return result;
	uintptr_t vtable = 0, getter = 0;
	if (!Peek(widget, vtable) || !vtable || !Peek(vtable + ItemAtCellSlot, getter)
		|| !NativeMemory::Executable(getter)) return result;
	// READ 2026-09-21 in the 3.3.93787 image, not yet measured live: BeltWidget
	// vtable 0x1CE2FA0 +0xC8 -> 0x227440 takes the cell's x alone as the belt
	// slot, 0 to 15, and returns the item there or null. The same getter slot
	// and calling convention the grid and equipment reads above already use.
	result.slot = cell.x;
	result.known = CallItemPointer(getter, widget, cell, result.item);
	return result;
}
}

BeltSlot BeltItem(uintptr_t widget) {
	if (!NativeWidgets::SupportsTree() || !Visible(widget) || Classify(widget).name != "BeltWidget") return {};
	const uint64_t selection = FocusSelection(widget);
	return QueryBelt(widget, {static_cast<int32_t>(selection), static_cast<int32_t>(selection >> 32)});
}

BeltSlot BeltItemAt(uintptr_t widget, int slot) { return QueryBelt(widget, {slot, 0}); }

std::vector<uintptr_t> Panels() {
	std::vector<uintptr_t> result;
	const uintptr_t manager = NativeWidgets::PanelManager();
	if (!NativeWidgets::SupportsTree() || !manager) return result;
	for (size_t offset : PanelVectors) {
		for (uintptr_t panel : VectorAt(manager + offset)) {
			if (panel && std::find(result.begin(), result.end(), panel) == result.end()) result.push_back(panel);
		}
	}
	return result;
}

namespace {
uintptr_t ListContainer(uintptr_t list) {
	// READ, 3.3.93787: a ListWidget (a ScrollViewWidget) creates an internal
	// widget named "Container" (0x140885C26) and keeps it at +0xD8
	// (0x140885C38); AddItem clones the row template into it (0x140885150) and
	// a panel fills row i as child i (AdvancedStatsPanel, 0x1414ED7C0). The
	// name is the check that `list` really has one.
	constexpr size_t ListContainerOffset = 0xD8;
	uintptr_t container = 0;
	if (!NativeWidgets::SupportsTree() || !list || !Peek(list + ListContainerOffset, container) || !container
		|| NativeWidgets::ReadName(container) != "Container") return 0;
	return container;
}
}

std::vector<uintptr_t> ListRows(uintptr_t list) {
	// No visibility filter: every child is a live row. The panel clears the list
	// before refilling it (0x140886250, from 0x1414EDBBD) and the list keeps the
	// hidden template outside the container, at +0xD0 (0x140885B15).
	std::vector<uintptr_t> rows;
	if (const uintptr_t container = ListContainer(list))
		for (uintptr_t row : VectorAt(container + ChildrenOffset)) if (row) rows.push_back(row);
	return rows;
}

std::string DescribeList(uintptr_t list) {
	const uintptr_t container = ListContainer(list);
	const auto rows = ListRows(list);
	uint8_t visible = 0xFF;
	if (!rows.empty()) (void)Peek(rows.front() + VisibleOffset, visible);
	char line[256];
	std::snprintf(line, sizeof(line), "list %llX type \"%s\", container %llX, %zu children, first \"%s\" visible=%d",
		static_cast<unsigned long long>(list), Classify(list).name.c_str(), static_cast<unsigned long long>(container),
		rows.size(), rows.empty() ? "" : NativeWidgets::ReadName(rows.front()).c_str(), visible);
	return line;
}

namespace {
std::atomic<FocusState> g_focusState{nullptr};
}
void SetFocusState(FocusState reader) { g_focusState = reader; }

UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t focus, uintptr_t named) {
	UiText::Snapshot snapshot;
	if (!Visible(panel)) return snapshot;
	snapshot.panel = panel;
	snapshot.focus = focus;
	if (!named) named = panel;
	snapshot.name = NativeWidgets::ReadName(named);
	snapshot.surface = Classify(panel).surface;
	std::unordered_set<uintptr_t> visited;
	Walk(panel, -1, 0, snapshot, visited);
	if (const auto reader = g_focusState.load())
		for (auto& node : snapshot.nodes) if (node.id == focus) node.state = reader(snapshot.name, named, node.name);
	// The visible journal graphic is information beside the objective, not
	// a text node. Append its plain name to our copied objective so selection,
	// manual repeat and the ordinary 150 ms stale-pane guard share one path.
	// Never read a persistent true-tomb global or write into a game widget.
	if (snapshot.surface == UiText::Surface::Menu && NativeWidgets::SupportsTombSymbols()) {
		constexpr size_t ImageFrameOffset = 0x90; // setter 0x859700
		UiText::Node* heading = nullptr;
		UiText::Node* objective = nullptr;
		for (auto& node : snapshot.nodes) if (node.kind == UiText::Kind::Text) {
			if (node.name == "ShortDescription") heading = &node;
			if (node.name == "LongDescription") objective = &node;
		}
		for (const auto& node : snapshot.nodes) {
			if (node.name != TombSymbols::WidgetName || node.type != "ImageWidget" || node.parent < 0
				|| !heading || !objective || UiText::Clean(objective->text).empty()) continue;
			const auto& owner = snapshot.nodes[static_cast<size_t>(node.parent)];
			if (owner.kind != UiText::Kind::QuestButton || owner.name != TombSymbols::QuestWidget
				|| UiText::Clean(heading->text) != UiText::Clean(owner.text)) continue;
			int32_t frame = -1;
			if (!Peek(node.id + ImageFrameOffset, frame) || frame < 0 || frame >= static_cast<int32_t>(TombSymbols::Count)) continue;
			objective->text = UiText::Clean(objective->text);
			objective->text += objective->text.ends_with('.') ? " " : ". ";
			objective->text += Strings::Current().tombSymbols[static_cast<size_t>(frame)];
			break;
		}
	}
	return snapshot;
}

std::vector<uintptr_t> FindControls(uintptr_t panel, UiText::Kind kind) {
	if (!Visible(panel)) return {};
	std::vector<uintptr_t> result, pending{panel};
	std::unordered_set<uintptr_t> visited;
	while (!pending.empty() && visited.size() < MaxNodes) {
		const uintptr_t widget = pending.back();
		pending.pop_back();
		if (!widget || !visited.insert(widget).second || NativeWidgets::ReadName(widget) == "Templates") continue;
		if (Classify(widget).kind == kind) result.push_back(widget);
		const auto children = VectorAt(widget + ChildrenOffset);
		for (auto it = children.rbegin(); it != children.rend() && pending.size() < MaxNodes; ++it) pending.push_back(*it);
	}
	return result;
}

void Reset() { g_types.clear(); }
}
