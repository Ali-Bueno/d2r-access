#include "native_ui.h"
#include "localization.h"
#include "native_memory.h"
#include "native_widgets.h"
#include "quest_log.h"

#include <windows.h>
#include <algorithm>
#include <array>
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
constexpr size_t SelectionOffset = 0x544; // FocusableWidget x/y, InventoryItemWidget 0x2A89C0
constexpr size_t GridDimensionsOffset = 0x628; // InventoryGridWidget update, 0x2C5FA0
constexpr size_t ItemAtCellSlot = 0xC8; // 0x2A89C0 calls this virtual getter with the selected x/y
constexpr int BeltSlots = 16; // the slot range BeltWidget's getter accepts, 0x227440
constexpr size_t FocusManagerOffset = 0xD0; // 0x846020
constexpr size_t FocusPanelOffset = 0x188, FocusWidgetOffset = 0x190; // 0x868230 / 0x876CA0
constexpr size_t PanelVectors[] = { 0x58, 0x88, 0xA0 }; // root children + FindPanel 0x89F760
// Bounds protect traversal from stale/corrupt pointers and cap UI-thread work.
constexpr size_t MaxNodes = 512, MaxDepth = 32, MaxTypeName = 96;
std::unordered_map<uintptr_t, Type> g_types;

uintptr_t CallType(uintptr_t function, uintptr_t widget) noexcept {
	__try {
		return reinterpret_cast<uintptr_t(*)(uintptr_t)>(function)(widget);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

bool CallItemPointer(uintptr_t function, uintptr_t widget, Cell cell, uintptr_t& item) noexcept {
	__try {
		item = reinterpret_cast<uintptr_t(*)(uintptr_t, const Cell*)>(function)(widget, &cell);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// The cell each archetype's +0xC8 getter takes; false when the widget has no
// item to read. InventoryGridWidget 0x2C49F0 queries the widget's owner/page
// and the selected cell (0x386A00 reads the grid). InventorySlotWidget's
// override, vtable 0x1CF3F20 -> 0x2CA060, ignores the cell and uses location
// +0x5D8 (0x3886D0). BeltWidget's, vtable 0x1CE2FA0 -> 0x227440, takes the
// cell's x alone as the slot -- READ 2026-09-21 in the 3.3.93787 image, not yet
// measured live. None of them moves an item.
bool ItemCell(uintptr_t widget, const Type& type, Cell& cell) {
	if (type.kind == Kind::InventorySlot) return true;
	if (type.kind == Kind::InventoryGrid) return (cell = GridCell(widget)).x >= 0;
	if (type.name != "BeltWidget") return false;
	const uint64_t selection = FocusSelection(widget);
	cell = {static_cast<int32_t>(selection), static_cast<int32_t>(selection >> 32)};
	return cell.x >= 0 && cell.x < BeltSlots;
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
	else if (type.kind == Kind::TabButton) node.text = NativeWidgets::ReadText(widget);
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

std::string InputText(uintptr_t widget) {
	if (!NativeWidgets::SupportsTree() || !widget) return {};
	const auto type = Classify(widget).name;
	if (type != "InputTextBoxWidget" && type != "NumericInputTextBoxWidget") return {};
	uintptr_t vtable = 0, getter = 0;
	std::array<uint8_t, InputTextGetterBytes.size()> instruction{};
	if (!Peek(widget, vtable) || !Peek(vtable + InputTextGetterSlot, getter)
		|| !NativeMemory::Executable(getter)
		|| !NativeMemory::Copy(getter + InputTextGetterInstruction, instruction.data(), instruction.size())
		|| instruction != InputTextGetterBytes) return {};
	std::string text;
	return NativeMemory::ReadEngineString(widget + InputTextOffset, text) ? text : std::string{};
}

ItemSlot QueryItem(uintptr_t widget, Cell cell) {
	ItemSlot result;
	uintptr_t vtable = 0, getter = 0;
	if (!Peek(widget, vtable) || !vtable || !Peek(vtable + ItemAtCellSlot, getter)
		|| !NativeMemory::Executable(getter)) return result;
	result.slot = cell.x;
	result.known = CallItemPointer(getter, widget, cell, result.item);
	return result;
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

Focus CurrentFocus() {
	if (!NativeWidgets::SupportsTree()) return {};
	uintptr_t manager = 0;
	Focus result;
	const uintptr_t panels = NativeWidgets::PanelManager();
	if (!panels || !Peek(panels + FocusManagerOffset, manager) || !manager
		|| !Peek(manager + FocusPanelOffset, result.panel) || !Peek(manager + FocusWidgetOffset, result.widget)) return {};
	if (!Visible(result.panel)) return {};
	if (!Visible(result.widget)) result.widget = 0;
	return result;
}

uint64_t FocusSelection(uintptr_t widget) {
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

ItemSlot FocusedItem(uintptr_t widget) {
	Cell cell;
	if (!NativeWidgets::SupportsTree() || !Visible(widget) || !ItemCell(widget, Classify(widget), cell)) return {};
	return QueryItem(widget, cell);
}

ItemSlot BeltItem(uintptr_t widget, int slot) {
	if (slot < 0 || slot >= BeltSlots || !NativeWidgets::SupportsTree()
		|| !Visible(widget) || Classify(widget).name != "BeltWidget") return {};
	return QueryItem(widget, {slot, 0});
}

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

UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t focus) {
	UiText::Snapshot snapshot;
	if (!Visible(panel)) return snapshot;
	snapshot.panel = panel;
	snapshot.focus = focus;
	snapshot.name = NativeWidgets::ReadName(panel);
	snapshot.surface = Classify(panel).surface;
	std::unordered_set<uintptr_t> visited;
	Walk(panel, -1, 0, snapshot, visited);
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
