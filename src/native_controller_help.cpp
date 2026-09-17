#include "native_controller_help.h"
#include "controller_text.h"
#include "native_memory.h"
#include "native_widgets.h"
#include <algorithm>

namespace D2RAccess::NativeControllerHelp {
namespace {
// 0x874270 adds and 0x873B00 removes active ButtonLegendWidget entries.
// 0x87B080 renders this same vector in order, including hold-to-use wording.
constexpr size_t EntriesOffset = 0x88, EntrySize = 0x40;
constexpr size_t GlyphCountOffset = 8, CaptionOffset = 0x10;
constexpr uint64_t MaxEntries = 32; // bounded read; scrolling legends display eight at once
constexpr int MaxChord = 2; // two uint32 glyph slots precede the count at +8
struct PanelLegends { uintptr_t panel = 0; std::vector<std::string> names; };
PanelLegends g_content, g_overlay;
void Append(std::string& text, const std::string& part) {
	if (part.empty()) return;
	if (!text.empty()) text += ". ";
	text += part;
}
// A scrolling legend (ControllerOverlay's "Legend") keeps its captions in the
// sub-text widgets the layout declares, named after the legend itself:
// Legend1..Legend8 for that one. They are read when the entry vector this
// class does not necessarily share with ButtonLegendWidget yields nothing, so
// the vendor's Buy/Sell/Repair and an item's own actions stay readable.
std::string SubTextHelp(uintptr_t widget) {
	const auto name = NativeWidgets::ReadName(widget);
	if (name.empty()) return {};
	std::string result;
	for (uint64_t i = 1; i <= MaxEntries; ++i) {
		const auto slot = NativeWidgets::FindChild(widget, name + std::to_string(i));
		if (!slot) break; // the game declares its slots contiguously
		if (!NativeUi::Visible(slot)) continue;
		Append(result, ControllerText::Read(NativeWidgets::ReadText(slot)));
	}
	return result;
}

std::string PanelHelp(uintptr_t panel, PanelLegends& cache) {
	if (!panel || !NativeUi::Visible(panel)) { cache = {}; return {}; }
	if (cache.panel != panel) {
		cache = {panel, {}};
		for (const auto widget : NativeUi::FindControls(panel, UiText::Kind::Legend)) {
			const auto name = NativeWidgets::ReadName(widget);
			if (!name.empty() && std::find(cache.names.begin(), cache.names.end(), name) == cache.names.end()) cache.names.push_back(name);
		}
	}
	// Cache lookup names only: FindChild resolves rebuilt controls on each poll.
	// Native layouts give their footer legends unique names within the panel.
	std::string result;
	for (const auto& name : cache.names) Append(result, Legend(NativeWidgets::FindChild(panel, name)));
	return result;
}
std::string LegendEntries(uintptr_t widget) {
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
}
std::string Legend(uintptr_t widget) {
	if (!NativeUi::Visible(widget) || NativeUi::Classify(widget).kind != UiText::Kind::Legend) return {};
	auto result = LegendEntries(widget);
	if (result.empty()) result = SubTextHelp(widget);
	return result;
}
std::string ForFocus(NativeUi::Focus focus) {
	const auto current = NativeUi::CurrentFocus();
	if (!focus.panel || !focus.widget || current.panel != focus.panel || current.widget != focus.widget) return {};
	std::string result = PanelHelp(focus.panel, g_content);
	// ControllerOverlay owns the shared footer, outside the focused content panel.
	const auto overlay = NativeWidgets::FindPanel("ControllerOverlay");
	if (overlay != focus.panel) Append(result, PanelHelp(overlay, g_overlay));
	return result;
}
void Reset() { g_content = {}; g_overlay = {}; }
}
