#include "native_ui.h"
#include "native_widgets.h"
#include "native_memory.h"
#include "speech_strings.h"
#include "test_support.h"

#include <array>
#include <cstring>
#include <windows.h>
#include <chrono>
#include <memory>

namespace {
uintptr_t managerAddress = 0;
bool supported = true, tombSupported = true;
uintptr_t cellItem = 0, queriedWidget = 0;
D2RAccess::NativeUi::Cell queriedCell;
bool itemQueryFault = false;
D2RAccess::NativeWidgets::Notification latestNotification;
struct Metadata { uintptr_t vtable = 0; const char* name = nullptr; Metadata* base = nullptr; };

uintptr_t TypeGetter(uintptr_t widget) {
	uintptr_t value = 0;
	D2RAccess::NativeMemory::Peek(widget + 0x38, value); // fixture-only slot
	return value;
}

uintptr_t ItemAtCell(uintptr_t widget, const D2RAccess::NativeUi::Cell* cell) {
	queriedWidget = widget; queriedCell = *cell;
	if (itemQueryFault) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
	return cellItem;
}

// Minimal synthetic native objects, using the layout recovered from the live
// functions in docs/native-ui.md. No game executable or screen reader needed.
struct Widget {
	std::array<unsigned char, 0x1800> data{};
	std::array<uintptr_t, 26> vtable{};
	Metadata metadata;
	std::string name, text;
	std::vector<uintptr_t> children;

	Widget(const char* type, const char* widgetName, const char* value = "") : metadata{0, type, nullptr}, name(widgetName), text(value) {
		vtable[11] = reinterpret_cast<uintptr_t>(&TypeGetter);
		Put(0, reinterpret_cast<uintptr_t>(vtable.data()));
		Put(0x38, reinterpret_cast<uintptr_t>(&metadata));
		Put<uint8_t>(0x50, 1);
		Put<uint8_t>(0x51, 1);
		StringAt(8, name);
		StringAt(0x88, text);
	}
	uintptr_t Address() { return reinterpret_cast<uintptr_t>(data.data()); }
	template <typename T> void Put(size_t offset, const T& value) { std::memcpy(data.data() + offset, &value, sizeof(T)); }
	void StringAt(size_t offset, const std::string& value) {
		Put(offset, reinterpret_cast<uintptr_t>(value.data()));
		Put<uint64_t>(offset + 8, value.size());
		Put<uint64_t>(offset + 16, (uint64_t{1} << 63) | value.size());
	}
	void Add(Widget& child) {
		children.push_back(child.Address());
		Put(0x58, reinterpret_cast<uintptr_t>(children.data()));
		Put<uint64_t>(0x60, children.size());
		child.Put(0x30, Address());
	}
};
}

// Substitute only the existing loader access layer; native_ui and native_memory
// are the actual production implementation under test.
namespace D2RAccess::NativeWidgets {
bool SupportsTree() { return supported; }
bool SupportsTombSymbols() { return supported && tombSupported; }
uintptr_t FindChild(uintptr_t parent, std::string_view name) {
	const auto* widget = reinterpret_cast<Widget*>(parent);
	for (uintptr_t child : widget->children) if (ReadName(child) == name) return child;
	return 0;
}
uintptr_t PanelManager() { return managerAddress; }
Notification ReadLatestNotification(uintptr_t) { return latestNotification; }
std::string ReadName(uintptr_t widget) {
	std::string value;
	return NativeMemory::ReadEngineString(widget + 8, value) ? value : "";
}
std::string ReadText(uintptr_t widget) {
	std::string value;
	return NativeMemory::ReadEngineString(widget + 0x88, value) ? value : "";
}
}

namespace {
void TombJournalSymbols() {
	using namespace D2RAccess;
	const auto language = Strings::Selected();
	Strings::Select(Strings::Language::enUS);
	Widget panel("QuestLogPanel", "QuestLogPanelExpansion"), quest("QuestLogButtonWidget", "Quest11", "The Seven Tombs");
	Widget title("TextBoxWidget", "ShortDescription", "The Seven Tombs");
	Widget objective("TextBoxWidget", "LongDescription", "The Symbol of the True Tomb of Tal Rasha.");
	Widget symbol("ImageWidget", "TalRashaSymbol");
	panel.Add(quest); quest.Add(symbol); panel.Add(title); panel.Add(objective);
	const auto read = [&] { return UiText::Describe(NativeUi::Snapshot(panel.Address(), quest.Address())); };
	const char* labels[] = {"four-pointed star", "square", "crescent", "circle", "double chevron with a diamond", "triangle", "circle above a crescent"};
	for (int32_t frame = 0; frame < 7; ++frame) {
		symbol.Put(0x90, frame);
		const auto reading = read();
		Check(reading.body == "The Seven Tombs" && reading.option == objective.text + " " + labels[frame],
			"each visible journal frame is plain text after the objective, with the title once");
		UiText::Reader reader;
		(void)reader.Update(reading, 0);
		(void)reader.Update(reading, 200);
		Check(reader.Repeat().find(labels[frame]) != std::string::npos,
			"manual repeat includes the journal symbol");
	}
	symbol.Put<uint8_t>(0x51, 0);
	Check(read().option == objective.text, "a hidden symbol never reveals the true tomb");
	symbol.Put<uint8_t>(0x51, 1); quest.Put<uint8_t>(0x51, 0);
	Check(UiText::Describe(NativeUi::Snapshot(panel.Address(), 0)).body.find("crescent") == std::string::npos,
		"a hidden quest ancestor also withholds its graphic");
	quest.Put<uint8_t>(0x51, 1);
	for (int32_t frame : {-1, 7, 100}) {
		symbol.Put(0x90, frame);
		Check(read().option == objective.text, "invalid frames fail closed without a guessed symbol");
	}
	symbol.Put<int32_t>(0x90, 3);
	tombSupported = false;
	Check(read().option == objective.text, "failed native symbol verification preserves ordinary journal text");
	tombSupported = true;
	const std::string staleTitle = "Another quest";
	title.StringAt(0x88, staleTitle);
	Check(read().option == Strings::Current().slotEmpty, "stale details do not acquire the selected quest's symbol");
	title.StringAt(0x88, title.text);
	objective.StringAt(0x88, "");
	Check(read().option == Strings::Current().questNoObjective, "a symbol never invents a missing objective");
	objective.StringAt(0x88, objective.text);
	Strings::Select(Strings::Language::esES);
	Check(read().option == objective.text + " círculo", "symbol vocabulary follows the selected text language");
	Strings::Select(language);
	NativeUi::Reset();
}
}

int main() {
	TombJournalSymbols();
	using namespace D2RAccess;
	Strings::Select(Strings::Language::esES);
	Widget manager("Widget", "PanelManager"), focusManager("FocusManager", "FocusManager");
	Widget panel("NpcDialogPanel", "NpcDialogPanel"), header("TextBoxWidget", "TextTemplate", "Akara");
	Widget list("ListWidget", "ListTemplate"), row("HoverListItemWidget", "ItemTemplate");
	Widget label("TextBoxWidget", "TextBox", "Hablar"), hidden("Widget", "Hidden");
	Widget hiddenText("TextBoxWidget", "HiddenText", "Never speak this");
	Widget templates("Widget", "Templates"), templateText("TextBoxWidget", "TextBox", "Template text");
	Widget button("ButtonWidget", "ButtonTemplate", "Salir");
	button.StringAt(0x8A0, button.text); // embedded textbox at +0x818, text +0x88
	manager.Add(panel);
	panel.Add(header); panel.Add(list); list.Add(row); row.Add(label);
	panel.Add(hidden); hidden.Add(hiddenText); hidden.Put<uint8_t>(0x51, 0);
	panel.Add(templates); templates.Add(templateText);
	panel.Add(button);
	managerAddress = manager.Address();
	manager.Put(0xD0, focusManager.Address());
	focusManager.Put(0x188, panel.Address());
	focusManager.Put(0x190, row.Address());
	const auto focus = NativeUi::CurrentFocus();
	Check(focus.panel == panel.Address() && focus.widget == row.Address(), "controller focus resolves panel and actual row");
	row.Put<NativeUi::Rect>(0x70, {7, 11, 120, 40});
	Check(NativeUi::LocalRect(row.Address()) == std::optional<NativeUi::Rect>{{7, 11, 120, 40}}, "native widget rect uses the reflected local bounds");
	Check(NativeUi::Children(list.Address()) == std::vector<uintptr_t>{row.Address()}, "direct children use the bounded native vector");
	row.Put<NativeUi::Rect>(0x70, {7, 11, -1, 40});
	Check(!NativeUi::LocalRect(row.Address()), "a negative native rect fails closed");
	Check(!NativeUi::LocalRect(1), "an unreadable native rect fails closed");
	row.Put<NativeUi::Rect>(0x70, {7, 11, 120, 40}); supported = false;
	Check(!NativeUi::LocalRect(row.Address()) && NativeUi::Children(list.Address()).empty(), "native layout access is disabled on an unsupported build");
	supported = true;
	Check(NativeUi::Panels() == std::vector<uintptr_t>{panel.Address()}, "manager children enumerate panels");
	auto snapshot = NativeUi::Snapshot(panel.Address(), focus.widget);
	Check(snapshot.surface == UiText::Surface::Menu, "runtime class selects menu archetype");
	Check(snapshot.nodes.size() == 6, "hidden branches and template containers are excluded");
	Check(UiText::Describe(snapshot).body == "Akara" && UiText::Describe(snapshot).option == "Hablar", "native tree yields NPC title and selected row");
	snapshot = NativeUi::Snapshot(panel.Address(), button.Address());
	Check(UiText::Describe(snapshot).option == "Salir", "button label comes from embedded textbox");
	panel.Put<uint8_t>(0x51, 0);
	Check(!NativeUi::Visible(label.Address()), "hidden ancestor hides descendants");
	Check(NativeUi::CurrentFocus().panel == 0, "hidden panel cannot own accessibility focus");
	Check(NativeUi::Snapshot(panel.Address(), row.Address()).nodes.empty(), "closed panel is not read");
	panel.Put<uint8_t>(0x51, 1);
	// EmoteWheelPanel stays open all game; closing the wheel only hides its Container.
	Widget wheel("EmoteWheelPanel", "EmoteWheelPanel"), wheelContent("Widget", "Container"), emote("ButtonWidget", "Emote2");
	wheel.Add(wheelContent); wheelContent.Add(emote);
	focusManager.Put(0x188, wheel.Address());
	focusManager.Put(0x190, emote.Address());
	Check(NativeUi::CurrentFocus().panel == wheel.Address() && NativeUi::CurrentFocus().widget == emote.Address(),
		"an open emote wheel holds focus on its button");
	wheelContent.Put<uint8_t>(0x51, 0);
	Check(NativeUi::CurrentFocus().panel == 0 && NativeUi::CurrentFocus().widget == 0,
		"a panel whose content is all hidden cannot hold focus");
	focusManager.Put<uintptr_t>(0x190, 0);
	Check(NativeUi::CurrentFocus().panel == 0, "nor with no focused widget at all");
	focusManager.Put(0x188, panel.Address());
	Check(NativeUi::CurrentFocus().panel == panel.Address() && NativeUi::CurrentFocus().widget == 0,
		"a panel showing content keeps focus with nothing focused");
	focusManager.Put(0x190, row.Address());
	row.Put<uint8_t>(0x50, 0);
	Check(UiText::Describe(NativeUi::Snapshot(panel.Address(), row.Address())).option == "Hablar. no disponible", "disabled rows remain readable");
	label.Put(0x30, label.Address());
	Check(!NativeUi::Visible(label.Address()), "parent cycles fail closed");
	label.Put(0x30, row.Address());
	row.Put<uint64_t>(0x60, 999999);
	Check(NativeUi::Snapshot(panel.Address(), row.Address()).nodes.size() == 5, "corrupt child counts are bounded");
	row.Put<uint64_t>(0x60, 1);
	Widget futureText("FutureTextWidget", "Future", "Still readable");
	Metadata textBase{0, "TextBoxWidget", nullptr};
	futureText.metadata.base = &textBase;
	Check(NativeUi::Classify(futureText.Address()).kind == UiText::Kind::Text, "future text subclasses inherit the reader");
	Widget futurePanel("UnknownSpeechPanel", "AnyFuturePanel"), speech("ScrollingSpeechWidget", "Speech"), speechText("TextBoxWidget", "ScrollingText", "A story");
	futurePanel.Add(speech); speech.Add(speechText);
	Check(NativeUi::Snapshot(futurePanel.Address(), 0).surface == UiText::Surface::Narrative, "scrolling speech archetype works in an unknown panel");
	Check(NativeUi::Classify(1).name.empty(), "invalid native widget does not fault");
	Widget feed("HUDMessagePanel", "HUDMessagePanel"), notifications("ChatListWidget", "Messages");
	Widget linkedText("TextBoxWithLinksWidget", "MessageText", "NoSDSwap notice");
	Metadata focusBase{0, "FocusableWidget", nullptr};
	linkedText.metadata.base = &focusBase;
	linkedText.StringAt(0x5F8, linkedText.text); // embedded textbox +0x570, string +0x88
	notifications.Put<uint32_t>(0x300, 1); // ChatNotificationMessages, not ChatHistory
	feed.Add(notifications); notifications.Add(linkedText);
	latestNotification = {12345, linkedText.text};
	snapshot = NativeUi::Snapshot(feed.Address(), 0);
	Check(snapshot.surface == UiText::Surface::NotificationFeed && snapshot.nodes[1].kind == UiText::Kind::NotificationList, "native notification feed and data source are identified");
	Check(snapshot.nodes[2].kind == UiText::Kind::LinkedText && snapshot.nodes[2].text == linkedText.text, "linked text uses its embedded textbox, not FocusableWidget fields");
	Check(snapshot.nodes[1].contentId == 12345 && snapshot.nodes[1].text == linkedText.text, "notification list snapshots include the actual latest delivery");
	notifications.Put<uint32_t>(0x300, 0);
	Check(NativeUi::Snapshot(feed.Address(), 0).nodes[1].kind == UiText::Kind::List, "chat history is excluded from notifications");
	linkedText.Put<uint8_t>(0x51, 0);
	Check(NativeUi::Snapshot(feed.Address(), 0).nodes.size() == 2, "invisible notification rows are omitted");
	Widget notice("NotificationPanel", "NotificationPanel");
	Check(NativeUi::Classify(notice.Address()).surface == UiText::Surface::Notice, "temporary notification panels share a notice reader");
	Widget grid("InventoryGridWidget", "grid"), slot("InventorySlotWidget", "head");
	Metadata itemBase{0, "InventoryItemWidget", &focusBase};
	grid.metadata.base = &itemBase; slot.metadata.base = &itemBase;
	grid.Put<uint64_t>(0x544, (uint64_t{3} << 32) | 2);
	grid.Put<int32_t>(0x628, 10); grid.Put<int32_t>(0x62C, 4);
	Check(NativeUi::Classify(grid.Address()).kind == UiText::Kind::InventoryGrid
		&& NativeUi::Classify(slot.Address()).kind == UiText::Kind::InventorySlot, "item subclasses retain their grid/slot archetypes");
	auto cell = NativeUi::GridCell(grid.Address());
	Check(cell.x == 2 && cell.y == 3, "inventory coordinates use actual native selection and dimensions");
	grid.vtable[25] = reinterpret_cast<uintptr_t>(&ItemAtCell);
	Check(NativeUi::GridItemPresence(grid.Address()) == NativeUi::ItemPresence::Empty, "grid lookup distinguishes a confirmed empty cell");
	cellItem = 123;
	Check(NativeUi::GridItemPresence(grid.Address()) == NativeUi::ItemPresence::Occupied
		&& queriedWidget == grid.Address() && queriedCell.x == 2 && queriedCell.y == 3, "grid lookup passes the native owner widget and selected coordinates");
	Check(NativeUi::GridItem(grid.Address()) == 123, "the grid names the item on the selected cell");
	Check(NativeUi::EquipmentItem(grid.Address()) == 0, "and a grid is not an equipment slot");
	Check(NativeUi::GridSize(grid.Address()).x == 10 && NativeUi::GridSize(grid.Address()).y == 4, "the grid reports its size in cells");
	Check(NativeUi::GridItemAt(grid.Address(), {9, 0}).known && queriedCell.x == 9 && queriedCell.y == 0, "any cell of the grid can be looked up for the census");
	Check(!NativeUi::GridItemAt(grid.Address(), {10, 0}).known, "a cell outside the grid never reaches the item getter");
	itemQueryFault = true;
	Check(NativeUi::GridItemPresence(grid.Address()) == NativeUi::ItemPresence::Unknown, "faulted item lookup is unknown rather than empty");
	itemQueryFault = false; grid.vtable[25] = 0;
	Check(NativeUi::GridItemPresence(grid.Address()) == NativeUi::ItemPresence::Unknown, "missing item getter is not called");
	grid.vtable[25] = reinterpret_cast<uintptr_t>(&ItemAtCell);
	grid.Put<uint8_t>(0x51, 0);
	Check(NativeUi::GridItemPresence(grid.Address()) == NativeUi::ItemPresence::Unknown, "hidden inventory is not queried");
	grid.Put<uint8_t>(0x51, 1);
	Check(NativeUi::GridItemPresence(slot.Address()) == NativeUi::ItemPresence::Unknown, "equipment does not use the grid item getter");
	grid.Put<int32_t>(0x544, 10);
	Check(NativeUi::GridCell(grid.Address()).x == -1, "out-of-range grid selection is ignored");
	Check(NativeUi::GridItemPresence(grid.Address()) == NativeUi::ItemPresence::Unknown, "out-of-range selection never reaches the item getter");
	grid.Put<int32_t>(0x628, 10000);
	Check(NativeUi::GridCell(grid.Address()).x == -1, "corrupt inventory dimensions are bounded");
	Check(NativeUi::GridCell(slot.Address()).x == -1, "equipment does not masquerade as a grid");
	Widget tabs("TabBarWidget", "AnyTabs"), tab0("TextBoxWidget", "Text0", "Personal"), tab1("TextBoxWidget", "Text1", "Comp.");
	tabs.Add(tab0); tabs.Add(tab1); tabs.Put<int32_t>(0x16C4, 1);
	Check(NativeUi::Snapshot(tabs.Address(), 0).nodes.front().text == "Comp.", "tabs read the selected runtime label");
	tabs.Put<int32_t>(0x544, 0);
	Check(NativeUi::SelectedTabText(tabs.Address()) == "Comp.", "tab activation index differs from the FocusableWidget hover index");
	Check(NativeUi::FocusedTabText(tabs.Address()) == "Personal", "the highlighted tab is read from the hover index");
	tabs.Put<int32_t>(0x544, 7);
	Check(NativeUi::FocusedTabText(tabs.Address()).empty(), "a hover index with no label reads nothing");
	tabs.Put<int32_t>(0x544, 0);
	Widget tabPanel("AnyPanel", "TabPanel"), container("Widget", "InitiallyHidden");
	tabPanel.Add(container); container.Add(tabs); container.Put<uint8_t>(0x51, 0);
	Check(NativeUi::FindControls(tabPanel.Address(), UiText::Kind::Tabs) == std::vector<uintptr_t>{tabs.Address()}, "discovery includes tabs whose ancestors start hidden");
	Check(NativeUi::SelectedTabText(tabs.Address()).empty(), "a hidden ancestor prevents a cached tab from being spoken");
	container.Put<uint8_t>(0x51, 1);
	Check(NativeUi::SelectedTabText(tabs.Address()) == "Comp.", "cached tab becomes readable when its ancestor opens");
	tabs.Put<int32_t>(0x16C4, -1);
	Check(NativeUi::Snapshot(tabs.Address(), 0).nodes.front().text.empty(), "unselected tabs have no inferred label");
	Widget target("MonsterHealthPanel", "HUDMonsterHealth");
	Check(NativeUi::Classify(target.Address()).surface == UiText::Surface::Target, "target-name panel is a passive interaction archetype");
	Widget goldButton("ButtonWidget", "gold_button", "Soltar oro");
	goldButton.StringAt(0x310, goldButton.text);
	Check(UiText::Describe(NativeUi::Snapshot(goldButton.Address(), goldButton.Address())).option == "Soltar oro", "icon-only button uses native tooltipString when its embedded caption is empty");
	Widget actionButton("ButtonWidget", "AnyAction");
	const std::string legend = "Usar pergamino", caption = "Acción", hint = "Descripción de la acción";
	actionButton.StringAt(0x310, legend);
	Check(UiText::Describe(NativeUi::Snapshot(actionButton.Address(), actionButton.Address())).option == legend, "controller legend names a button with neither caption nor tooltip");
	actionButton.StringAt(0x8A0, caption); actionButton.StringAt(0x88, hint);
	Check(UiText::Describe(NativeUi::Snapshot(actionButton.Address(), actionButton.Address())).option == caption + ". " + hint, "visible button caption precedes its own tooltip");
	actionButton.StringAt(0x88, caption);
	Check(UiText::Describe(NativeUi::Snapshot(actionButton.Address(), actionButton.Address())).option == caption, "identical caption and tooltip are not duplicated");
	actionButton.Put<uint8_t>(0x50, 0);
	Check(UiText::Describe(NativeUi::Snapshot(actionButton.Address(), actionButton.Address())).option == caption + ". no disponible", "native action help retains unavailable state");
	Widget toggle("ToggleButtonWidget", "AnyToggle"); Metadata buttonBase{0, "ButtonWidget", &focusBase}; toggle.metadata.base = &buttonBase;
	Check(NativeUi::Classify(toggle.Address()).kind == UiText::Kind::Toggle, "toggle archetype is preserved before walking its ButtonWidget base");
	Widget dropdown("DropdownListWidget", "WindowMode"); dropdown.metadata.base = &buttonBase;
	Check(NativeUi::Classify(dropdown.Address()).kind == UiText::Kind::ValueControl, "dropdown buttons stay with the value reader");
	Widget carousel("CarouselWidget", "ItemNameDisplayMode"); carousel.metadata.base = &focusBase;
	Check(NativeUi::Classify(carousel.Address()).kind == UiText::Kind::ValueControl, "carousels retain value semantics");
	Widget statButton("CharacterStatLevelUpButtonWidget", "StrengthPointButton", "Gastar punto"); statButton.metadata.base = &buttonBase;
	Check(NativeUi::Classify(statButton.Address()).kind == UiText::Kind::StatButton, "stat action keeps its role ahead of the button base");
	Widget skillTab("TabWidget", "Tab0", "Gritos de guerra"); skillTab.metadata.base = &focusBase;
	Check(NativeUi::Classify(skillTab.Address()).kind == UiText::Kind::TabButton, "individual tab retains its role ahead of the focusable base");
	Check(UiText::Describe(NativeUi::Snapshot(skillTab.Address(), skillTab.Address())).option == "Gritos de guerra", "individual focused tabs speak native labels");
	skillTab.Add(statButton);
	Check(NativeUi::Parent(statButton.Address()) == skillTab.Address(), "companion reader can resolve the actual parent");
	statButton.Put(0x30, statButton.Address());
	Check(NativeUi::Parent(statButton.Address()) == 0, "self-parent pointers fail closed");
	slot.Put<int32_t>(0x5D8, 4);
	slot.vtable[25] = reinterpret_cast<uintptr_t>(&ItemAtCell);
	cellItem = 123;
	Check(NativeUi::EquipmentItemPresence(slot.Address()) == NativeUi::ItemPresence::Occupied
		&& queriedWidget == slot.Address(), "equipment query uses its own native owner and getter");
	Check(NativeUi::EquipmentItem(slot.Address()) == 123, "an equipment slot names the item it holds");
	cellItem = 0;
	Check(NativeUi::EquipmentItemPresence(slot.Address()) == NativeUi::ItemPresence::Empty, "native equipment lookup confirms an empty slot");
	itemQueryFault = true;
	Check(NativeUi::EquipmentItemPresence(slot.Address()) == NativeUi::ItemPresence::Unknown, "equipment lookup failure never announces an empty slot");
	Check(NativeUi::EquipmentItem(slot.Address()) == 0, "and names no item");
	itemQueryFault = false;
	Check(NativeUi::EquipmentItemPresence(grid.Address()) == NativeUi::ItemPresence::Unknown, "equipment lookup excludes grid widgets");
	Check(NativeUi::EquipmentLocation(slot.Address()) == 4, "equipment slot location uses the reflected body location member");
	Check(NativeUi::EquipmentLocation(goldButton.Address()) == 0, "button memory is never interpreted as an equipment location");
	slot.Put<uint8_t>(0x51, 0);
	Check(NativeUi::EquipmentItemPresence(slot.Address()) == NativeUi::ItemPresence::Unknown, "hidden equipment is not queried");
	Check(NativeUi::EquipmentLocation(slot.Address()) == 0, "hidden equipment has no readable location");
	// The belt shares that getter, numbering its slots by the selection's x.
	Widget belt("BeltWidget", "belt");
	belt.metadata.base = &itemBase;
	belt.vtable[25] = reinterpret_cast<uintptr_t>(&ItemAtCell);
	belt.Put<uint64_t>(0x544, 3);
	cellItem = 123;
	auto focused = NativeUi::BeltItem(belt.Address());
	Check(focused.known && focused.item == cellItem && focused.slot == 3
		&& queriedWidget == belt.Address() && queriedCell.x == 3, "a belt slot returns the unit in the selected slot");
	belt.Put<uint64_t>(0x544, NativeUi::BeltSlots);
	Check(!NativeUi::BeltItem(belt.Address()).known, "a selection past the last belt slot is never queried");
	focused = NativeUi::BeltItemAt(belt.Address(), 11);
	Check(focused.known && focused.slot == 11 && focused.item == cellItem && queriedCell.x == 11
		&& NativeUi::FocusSelection(belt.Address()) == NativeUi::BeltSlots,
		"reading a reserve cell leaves the native belt selection untouched");
	Check(!NativeUi::BeltItemAt(belt.Address(), -1).known && !NativeUi::BeltItemAt(belt.Address(), NativeUi::BeltSlots).known
		&& !NativeUi::BeltItemAt(grid.Address(), 0).known, "belt queries reject invalid slots and other widget types");
	itemQueryFault = true;
	Check(!NativeUi::BeltItemAt(belt.Address(), 4).known, "a failed belt reserve read remains unknown rather than empty");
	itemQueryFault = false;
	Widget footer("ButtonLegendScrollWidget", "AnyFooter");
	Metadata legendBase{0, "ButtonLegendWidget", nullptr}; footer.metadata.base = &legendBase;
	Check(NativeUi::Classify(footer.Address()).kind == UiText::Kind::Legend, "scrolling controller legends retain the shared legend archetype");
	// These executable fixtures contain only the verified member-load bytes;
	// production reads the member but never calls the getter.
	auto* getterCode = static_cast<unsigned char*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	Check(getterCode != nullptr, "executable getter fixture allocated");
	const unsigned char inputGetter[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0x81, 0x58, 0x05, 0x00, 0x00};
	const unsigned char toggleGetter[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x0F, 0xB6, 0x91, 0x88, 0x0B, 0x00, 0x00};
	std::memcpy(getterCode, inputGetter, sizeof(inputGetter));
	std::memcpy(getterCode + 32, toggleGetter, sizeof(toggleGetter));
	Widget input("InputTextBoxWidget", "SearchInput", "wrong offset");
	std::string entered = "Partida de Ana";
	input.StringAt(0x558, entered);
	input.vtable[0x70 / sizeof(uintptr_t)] = reinterpret_cast<uintptr_t>(getterCode);
	Check(NativeUi::InputText(input.Address()) == entered, "native input reads current text at verified +0x558");
	Check(NativeUi::Snapshot(input.Address(), input.Address()).nodes.front().text == entered,
		"input snapshot uses current contents rather than the +0x88 tooltip field");
	getterCode[12] = 0x01;
	Check(NativeUi::InputText(input.Address()).empty(), "changed getter fingerprint blocks input offset read");
	getterCode[12] = 0x00;
	Widget numeric("NumericInputTextBoxWidget", "MaxPlayersInput");
	std::string playerCount = "4";
	numeric.StringAt(0x558, playerCount);
	numeric.vtable[0x70 / sizeof(uintptr_t)] = reinterpret_cast<uintptr_t>(getterCode);
	Check(NativeUi::InputText(numeric.Address()) == playerCount, "numeric input shares the guarded text getter");
	Widget masked("MaskedInputTextBoxWidget", "PasswordInput");
	std::string password = "secret";
	masked.StringAt(0x558, password);
	masked.vtable[0x70 / sizeof(uintptr_t)] = reinterpret_cast<uintptr_t>(getterCode);
	Check(NativeUi::InputText(masked.Address()).empty()
		&& NativeUi::Snapshot(masked.Address(), masked.Address()).nodes.front().text.empty(),
		"masked input is never read into speech snapshots");
	Widget multiplayerToggle("ToggleButtonWidget", "UnlistedCheckbox");
	multiplayerToggle.vtable[0x8 / sizeof(uintptr_t)] = reinterpret_cast<uintptr_t>(getterCode + 32);
	multiplayerToggle.Put<uint8_t>(0xB88, 1);
	Check(NativeUi::Snapshot(multiplayerToggle.Address(), multiplayerToggle.Address()).nodes.front().toggled == true,
		"toggle uses verified isToggled byte");
	multiplayerToggle.Put<uint8_t>(0xB88, 0);
	Check(NativeUi::Snapshot(multiplayerToggle.Address(), multiplayerToggle.Address()).nodes.front().toggled == false,
		"toggle reports its current unchecked state");
	getterCode[32 + 12] = 0x01;
	Check(!NativeUi::Snapshot(multiplayerToggle.Address(), multiplayerToggle.Address()).nodes.front().toggled,
		"changed toggle getter fingerprint blocks state read");
	Widget difficulty("DropdownListWidget", "BrowseDifficultyFilterDropdown", "Filter by difficulty");
	std::string selectedDifficulty = "Normal";
	difficulty.StringAt(0x8A0, selectedDifficulty);
	const auto difficultyNode = NativeUi::Snapshot(difficulty.Address(), difficulty.Address()).nodes.front();
	Check(difficultyNode.text == "Normal" && difficultyNode.hint == "Filter by difficulty",
		"native dropdown keeps selected caption separate from its tooltip");
	VirtualFree(getterCode, 0, MEM_RELEASE);
	// Synthetic busy inventory panel: report traversal cost, without a timing
	// assertion that would fail under debugger or machine load.
	Widget busy("Widget", "BusyInventory");
	std::vector<std::unique_ptr<Widget>> labels;
	constexpr int BenchmarkNodes = 256, BenchmarkPasses = 40;
	for (int i = 0; i < BenchmarkNodes; ++i) {
		labels.push_back(std::make_unique<Widget>("TextBoxWidget", "Property", "Durabilidad: 28 de 28"));
		busy.Add(*labels.back());
	}
	Check(NativeUi::Snapshot(busy.Address(), 0).nodes.size() == BenchmarkNodes + 1, "benchmark traverses the complete busy panel");
	auto benchmark = [&](bool scoped) {
		const auto start = std::chrono::steady_clock::now();
		for (int i = 0; i < BenchmarkPasses; ++i) {
			if (scoped) { const NativeMemory::ReadScope pass; (void)NativeUi::Snapshot(busy.Address(), 0); }
			else (void)NativeUi::Snapshot(busy.Address(), 0);
		}
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / BenchmarkPasses;
	};
	const auto uncached = benchmark(false), scoped = benchmark(true);
	std::printf("Busy UI snapshot (%d widgets): %.3f ms uncached, %.3f ms scoped\n", BenchmarkNodes + 1, uncached, scoped);
	Widget rowList("ListWidget", "ListContainer"), rowContainer("Widget", "Container");
	Widget shownRow("ListItemWidget", "ListItem0"), hiddenRow("ListItemWidget", "ListItem1");
	rowContainer.Add(shownRow); rowContainer.Add(hiddenRow); hiddenRow.Put<uint8_t>(0x51, 0);
	rowList.Put(0xD8, rowContainer.Address());
	Check(NativeUi::ListRows(rowList.Address()) == std::vector<uintptr_t>{shownRow.Address(), hiddenRow.Address()},
		"a list's rows are all of its container's children, shown or not");
	Check(NativeUi::ListRows(rowContainer.Address()).empty(), "a widget with no row container has no rows");
	Widget otherList("ListWidget", "ListContainer"), notContainer("Widget", "View");
	notContainer.Add(shownRow);
	otherList.Put(0xD8, notContainer.Address());
	Check(NativeUi::ListRows(otherList.Address()).empty(), "only a widget the list named Container is read as its rows");
	Check(NativeUi::DescribeList(rowList.Address()).find("2 children, first \"ListItem0\" visible=1") != std::string::npos,
		"the log line names the container's children");
	supported = false;
	Check(NativeUi::Panels().empty() && NativeUi::Snapshot(panel.Address(), 0).nodes.empty(), "unverified game layout disables traversal");
	NativeUi::Reset();
	Passed();
}
