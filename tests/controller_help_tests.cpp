#include "native_controller_help.h"
#include "controller_text.h"
#include "speech_strings.h"
#include "test_support.h"
#include <array>
#include <cstring>
#include <iterator>
#include <string>

namespace {
using namespace D2RAccess;
// ButtonLegendWidget vector and records, as consumed by 0x87B080.
std::array<unsigned char, 0x98> legend{};
std::array<unsigned char, 0x98> rebuiltLegend{};
std::array<unsigned char, 3 * 0x40> records{};
bool visible = true;
bool rebuilt = false;
// Sub-text slots a ButtonLegendScrollWidget declares: ControllerOverlayHD.json
// names them after the legend itself (Legend1..Legend8) and the game fills them
// with the actions it is displaying.
const char* slots[3]{};
int hiddenSlot = -1;
void Slots(const char* first = nullptr, const char* second = nullptr, const char* third = nullptr) {
	slots[0] = first; slots[1] = second; slots[2] = third;
}
uintptr_t SlotAddress(size_t index) { return 100 + index; }
int discoveries = 0;
NativeUi::Focus focus{1, 2};
uintptr_t Address() { return reinterpret_cast<uintptr_t>(rebuilt ? rebuiltLegend.data() : legend.data()); }
template<class T> void Put(unsigned char* target, size_t offset, T value) { std::memcpy(target + offset, &value, sizeof(value)); }
void Entry(size_t index, uint32_t first, uint32_t second, int count, const char* caption) {
	auto* entry = records.data() + index * 0x40;
	Put(entry, 0, first); Put(entry, 4, second); Put(entry, 8, count);
	Put(entry, 0x10, reinterpret_cast<uintptr_t>(caption));
	Put(entry, 0x18, static_cast<uint64_t>(std::strlen(caption)));
	Put(entry, 0x20, static_cast<uint64_t>(std::strlen(caption))); // engine-string capacity
}
}
namespace D2RAccess::NativeUi {
bool Visible(uintptr_t widget) {
	for (size_t i = 0; i < std::size(slots); ++i)
		if (widget == SlotAddress(i)) return hiddenSlot != static_cast<int>(i);
	return widget && visible;
}
Type Classify(uintptr_t widget) { return {"ButtonLegendScrollWidget", widget == Address() ? UiText::Kind::Legend : UiText::Kind::Other}; }
Focus CurrentFocus() { return focus; }
std::vector<uintptr_t> FindControls(uintptr_t panel, UiText::Kind kind) {
	++discoveries;
	return panel == 3 && kind == UiText::Kind::Legend ? std::vector<uintptr_t>{Address()} : std::vector<uintptr_t>{};
}
}
namespace D2RAccess::NativeWidgets {
uintptr_t FindPanel(std::string_view name) { return name == "ControllerOverlay" ? 3 : 0; }
uintptr_t FindChild(uintptr_t parent, std::string_view name) {
	if (parent == 3 && name == "Legend") return Address();
	if (parent != Address()) return 0;
	for (size_t i = 0; i < std::size(slots); ++i)
		if (slots[i] && name == "Legend" + std::to_string(i + 1)) return SlotAddress(i);
	return 0;
}
std::string ReadName(uintptr_t widget) { return widget == Address() ? "Legend" : ""; }
std::string ReadText(uintptr_t widget) {
	for (size_t i = 0; i < std::size(slots); ++i)
		if (widget == SlotAddress(i) && slots[i]) return slots[i];
	return {};
}
}
int main() {
	using namespace D2RAccess;
	Strings::Select(Strings::Language::esES);
	Check(ControllerText::Read("\xEE\x80\x8F") == "A", "live Porrazo binding glyph reads A");
	Check(ControllerText::Read("\xEE\x80\x8D+\xEE\x80\x91") == "LT + X", "alternate skill bindings preserve the modifier");
	Check(ControllerText::Read("\xEE\x80\x91" "Asignar habilidad") == "X Asignar habilidad", "adjacent caption keeps its controller button");
	Check(ControllerText::Read("\xEE\x81\xBF" "Texto") == "Texto", "unknown formatting glyphs are not guessed as buttons");
	Check(ControllerText::Button(0).empty() && ControllerText::Button(0xE07E).empty(), "non-button and skill font commands have no button label");
	Put(legend.data(), 0x88, reinterpret_cast<uintptr_t>(records.data()));
	Put(legend.data(), 0x90, uint64_t{2});
	Entry(0, 0xE011, 0, 1, "Asignar habilidad"); Entry(1, 0xE010, 0, 1, "Cerrar menú");
	Check(NativeControllerHelp::Legend(Address()) == "X: Asignar habilidad. B: Cerrar menú", "read-only fixture matches Porrazo's live legend");
	Check(NativeControllerHelp::ForFocus({1, 2}) == "X: Asignar habilidad. B: Cerrar menú", "focused content gets its shared controller footer");
	const int initialDiscoveries = discoveries;
	focus.widget = 4;
	Check(NativeControllerHelp::ForFocus(focus) == "X: Asignar habilidad. B: Cerrar menú"
		&& discoveries == initialDiscoveries, "moving within a panel refreshes actions without another full tree walk");
	rebuiltLegend = legend; rebuilt = true;
	Entry(0, 0xE012, 0, 1, "Reparar todo");
	Check(NativeControllerHelp::ForFocus(focus) == "Y: Reparar todo. B: Cerrar menú"
		&& discoveries == initialDiscoveries, "a rebuilt footer is resolved by name and supplies its current action");
	rebuilt = false; focus.widget = 2;
	Check(NativeControllerHelp::ForFocus({1, 9}).empty(), "covered or stale focus cannot read another control's actions");
	Entry(0, 0xE00F, 0, 1, "Mantén pulsado para asignar punto(s) de habilidad");
	Check(NativeControllerHelp::Legend(Address()).starts_with("A: Mantén pulsado"), "native hold-to-spend wording is retained");
	Entry(0, 0xE00D, 0xE011, 2, "Acción");
	Check(NativeControllerHelp::Legend(Address()).starts_with("LT + X: Acción"), "two-button legend chords retain native order");
	Entry(0, 0xFFFF, 0, 1, "Acción");
	Check(NativeControllerHelp::Legend(Address()) == "B: Cerrar menú", "unknown button does not create a false instruction");
	Entry(0, 0xE00D, 0, 1, "");
	Check(NativeControllerHelp::Legend(Address()) == "B: Cerrar menú", "captionless tab indicators are not mistaken for actions");
	Entry(0, 0xE00D, 0, 3, "Acción");
	Check(NativeControllerHelp::Legend(Address()) == "B: Cerrar menú", "corrupt chord count is rejected");
	visible = false; Check(NativeControllerHelp::Legend(Address()).empty(), "hidden legends do not announce stale actions");
	visible = true; Put(legend.data(), 0x90, uint64_t{0});
	Check(NativeControllerHelp::Legend(Address()).empty(), "removed actions disappear immediately");
	Put(legend.data(), 0x90, uint64_t{10000});
	Check(NativeControllerHelp::Legend(Address()).empty(), "corrupt vector is bounded before reading");
	Check(NativeControllerHelp::Legend(1).empty(), "non-legend archetypes never read legend offsets");
	// A ButtonLegendScrollWidget is a different class and need not share
	// ButtonLegendWidget's entry vector; its captions are then read from the
	// sub-text slots its layout declares, which is where the vendor's actions live.
	Slots("\xEE\x80\x8F" "Comprar", "\xEE\x80\x91" "Vender", "\xEE\x80\x92" "Reparar");
	Check(NativeControllerHelp::Legend(Address()) == "A Comprar. X Vender. Y Reparar",
		"a scrolling legend with an unreadable entry vector still announces every action it displays");
	hiddenSlot = 1;
	Check(NativeControllerHelp::Legend(Address()) == "A Comprar. Y Reparar", "a slot the game hides is not announced");
	hiddenSlot = -1;
	Slots("\xEE\x80\x8F" "Comprar", nullptr, "\xEE\x80\x92" "Reparar");
	Check(NativeControllerHelp::Legend(Address()) == "A Comprar", "the walk stops at the first slot the widget does not declare");
	visible = false;
	Check(NativeControllerHelp::Legend(Address()).empty(), "a hidden scrolling legend announces neither entries nor slots");
	visible = true;
	Put(legend.data(), 0x90, uint64_t{1}); Entry(0, 0xE010, 0, 1, "Cerrar menú");
	Check(NativeControllerHelp::Legend(Address()) == "B: Cerrar menú", "the slots stay a fallback: a readable entry vector still wins");
	Slots();
	NativeControllerHelp::Reset();
	Put(legend.data(), 0x90, uint64_t{1}); Entry(0, 0xE011, 0, 1, "Actualizar");
	Check(NativeControllerHelp::ForFocus(focus) == "X: Actualizar" && discoveries > initialDiscoveries, "session reset rediscovers new NPC controls");
	Check(ControllerText::WithoutControl("A: Hold to Assign One Stat Point. View: View Advanced Stats. B: Close Menu", "View")
		== "A: Hold to Assign One Stat Point. B: Close Menu", "the entry for a button the mod has taken is left out");
	Check(ControllerText::WithoutControl("View: View Advanced Stats", "View").empty(), "and a list of that entry alone is empty");
	Check(ControllerText::WithoutControl("A: Select. B: Close Menu", "View") == "A: Select. B: Close Menu", "other lists pass unchanged");
	Passed();
}
