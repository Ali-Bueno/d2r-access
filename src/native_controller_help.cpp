#include "native_controller_help.h"
#include "controller_text.h"
#include "native_memory.h"
#include "native_widgets.h"

namespace D2RAccess::NativeControllerHelp {
namespace {
// 0x874270 adds and 0x873B00 removes active ButtonLegendWidget entries.
// 0x87B080 renders this same vector in order, including hold-to-use wording.
constexpr size_t EntriesOffset = 0x88, EntrySize = 0x40;
constexpr size_t GlyphCountOffset = 8, CaptionOffset = 0x10;
constexpr uint64_t MaxEntries = 32; // bounded read; scrolling legends display eight at once
constexpr int MaxChord = 2; // two uint32 glyph slots precede the count at +8
void Append(std::string& text, const std::string& part) {
	if (part.empty()) return;
	if (!text.empty()) text += ". ";
	text += part;
}
}
std::string Legend(uintptr_t widget) {
	if (!NativeUi::Visible(widget) || NativeUi::Classify(widget).kind != UiText::Kind::Legend) return {};
	struct { uintptr_t data; uint64_t count; } entries{};
	if (!NativeMemory::Peek(widget + EntriesOffset, entries) || !entries.data || entries.count > MaxEntries) return {};
	std::string result;
	for (uint64_t i = 0; i < entries.count; ++i) {
		const auto entry = entries.data + i * EntrySize;
		int count = 0;
		std::string caption;
		if (!NativeMemory::Peek(entry + GlyphCountOffset, count) || count < 1 || count > MaxChord
			|| !NativeMemory::ReadEngineString(entry + CaptionOffset, caption)) continue;
		caption = ControllerText::Read(caption);
		if (caption.empty()) continue; // standalone tab icons have no action caption
		std::string chord;
		for (int j = 0; j < count; ++j) {
			uint32_t glyph = 0;
			if (!NativeMemory::Peek(entry + j * sizeof(glyph), glyph) || ControllerText::Button(glyph).empty()) { chord.clear(); break; }
			if (!chord.empty()) chord += " + ";
			chord += ControllerText::Button(glyph);
		}
		if (!chord.empty()) Append(result, chord + ": " + caption);
	}
	return result;
}
std::string ForFocus(NativeUi::Focus focus) {
	const auto current = NativeUi::CurrentFocus();
	if (!focus.panel || !focus.widget || current.panel != focus.panel || current.widget != focus.widget) return {};
	std::string result;
	for (const auto legend : NativeUi::FindControls(focus.panel, UiText::Kind::Legend)) Append(result, Legend(legend));
	// ControllerOverlay owns the shared footer, outside the focused content panel.
	const auto overlay = NativeWidgets::FindPanel("ControllerOverlay");
	if (overlay && overlay != focus.panel && NativeUi::Visible(overlay))
		for (const auto legend : NativeUi::FindControls(overlay, UiText::Kind::Legend)) Append(result, Legend(legend));
	return result;
}
}
