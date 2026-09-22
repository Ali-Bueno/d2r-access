#include "native_ui.h"
#include "localization.h"
#include "native_memory.h"
#include "native_widgets.h"
#include "quest_log.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
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
constexpr size_t ChildrenOffset = 0x58; // FindChildWidget 0x855C90
constexpr size_t GetTypeSlot = 0x58;    // vtable, distinct from the instance member
constexpr size_t TypeNameOffset = 0x8, BaseTypeOffset = 0x10; // 0x851030
constexpr size_t ButtonTextOffset = 0x818; // 0x8496CA calls SetText on embedded TextBoxWidget
constexpr size_t ButtonLegendOffset = 0x310; // FocusableWidget registration 0x875CB0; live gold_button
constexpr size_t EquipmentLocationOffset = 0x5D8; // InventorySlotWidget location, live metadata 0x236C530
constexpr size_t LinkedTextOffset = 0x570; // 0x87DC10 / 0x87EFE0 delegate to embedded TextBoxWidget
constexpr size_t ChatDataSourceOffset = 0x300; // ChatListWidget::Count, 0x84CC70
constexpr uint32_t ChatNotificationMessages = 1; // ChatDataSource registration, 0x8BA3A0
constexpr size_t SelectedTabOffset = 0x16C4; // TabBarWidget input handler, 0x878D30
constexpr size_t SelectionOffset = 0x544; // FocusableWidget x/y, InventoryItemWidget 0x2A89C0
constexpr size_t GridDimensionsOffset = 0x628; // InventoryGridWidget update, 0x2C5FA0
constexpr size_t ItemAtCellSlot = 0xC8; // 0x2A89C0 calls this virtual getter with the selected x/y
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
	if (type.kind == Kind::Text) node.text = NativeWidgets::ReadText(widget);
	else if (type.kind == Kind::LinkedText) node.text = NativeWidgets::ReadText(widget + LinkedTextOffset);
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

BeltSlot BeltItem(uintptr_t widget) {
	BeltSlot result;
	if (!NativeWidgets::SupportsTree() || !Visible(widget) || Classify(widget).name != "BeltWidget") return result;
	const uint64_t selection = FocusSelection(widget);
	const Cell cell{static_cast<int32_t>(selection), static_cast<int32_t>(selection >> 32)};
	if (cell.x < 0 || cell.x >= 16) return result;
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
