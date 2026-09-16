#include "native_controller_help.h"
#include "controller_text.h"
#include "speech_strings.h"
#include "test_support.h"
#include <array>
#include <cstring>

namespace {
using namespace D2RAccess;
// ButtonLegendWidget vector and records, as consumed by 0x87B080.
std::array<unsigned char, 0x98> legend{};
std::array<unsigned char, 3 * 0x40> records{};
bool visible = true;
NativeUi::Focus focus{1, 2};
uintptr_t Address() { return reinterpret_cast<uintptr_t>(legend.data()); }
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
bool Visible(uintptr_t widget) { return widget && visible; }
Type Classify(uintptr_t widget) { return {"ButtonLegendScrollWidget", widget == Address() ? UiText::Kind::Legend : UiText::Kind::Other}; }
Focus CurrentFocus() { return focus; }
std::vector<uintptr_t> FindControls(uintptr_t panel, UiText::Kind kind) {
	return panel == 3 && kind == UiText::Kind::Legend ? std::vector<uintptr_t>{Address()} : std::vector<uintptr_t>{};
}
}
namespace D2RAccess::NativeWidgets {
uintptr_t FindPanel(std::string_view name) { return name == "ControllerOverlay" ? 3 : 0; }
}
int main() {
	using namespace D2RAccess;
	Strings::Select(Strings::Language::Spanish);
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
	Passed();
}
