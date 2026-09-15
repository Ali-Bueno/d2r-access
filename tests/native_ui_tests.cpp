#include "native_ui.h"
#include "native_widgets.h"
#include "native_memory.h"
#include "speech_strings.h"
#include "test_support.h"

#include <array>
#include <cstring>

namespace {
uintptr_t managerAddress = 0;
bool supported = true;
struct Metadata { uintptr_t vtable = 0; const char* name = nullptr; Metadata* base = nullptr; };

uintptr_t TypeGetter(uintptr_t widget) {
	uintptr_t value = 0;
	D2RAccess::NativeMemory::Peek(widget + 0x38, value); // fixture-only slot
	return value;
}

// Minimal synthetic native objects, using the layout recovered from the live
// functions in docs/native-ui.md. No game executable or screen reader needed.
struct Widget {
	std::array<unsigned char, 0xB00> data{};
	std::array<uintptr_t, 12> vtable{};
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
uintptr_t PanelManager() { return managerAddress; }
std::string ReadName(uintptr_t widget) {
	std::string value;
	return NativeMemory::ReadEngineString(widget + 8, value) ? value : "";
}
std::string ReadText(uintptr_t widget) {
	std::string value;
	return NativeMemory::ReadEngineString(widget + 0x88, value) ? value : "";
}
}

int main() {
	using namespace D2RAccess;
	Strings::Select(Strings::Language::Spanish);
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
	supported = false;
	Check(NativeUi::Panels().empty() && NativeUi::Snapshot(panel.Address(), 0).nodes.empty(), "unverified game layout disables traversal");
	NativeUi::Reset();
	Passed();
}
