#include "controller_text.h"
#include "speech_strings.h"
#include "ui_text.h"

namespace D2RAccess::ControllerText {
namespace {
// Control symbols are private-use code points, three bytes in UTF-8.
bool GlyphAt(std::string_view text, size_t i, uint32_t& glyph) {
	if (i + 2 >= text.size()) return false;
	const auto a = static_cast<unsigned char>(text[i]);
	const auto b = static_cast<unsigned char>(text[i + 1]);
	const auto c = static_cast<unsigned char>(text[i + 2]);
	if (a != 0xEE || (b & 0xC0) != 0x80 || (c & 0xC0) != 0x80) return false;
	glyph = ((a & 0x0F) << 12) | ((b & 0x3F) << 6) | (c & 0x3F);
	return true;
}
}
std::string_view Button(uint32_t glyph) {
	// ButtonLegendWidget's registration at 0x872E50 maps these consecutive
	// font codes to physical controls. Xbox face labels match the live UI;
	// 0x1441F0 confirms the corresponding XInput button masks.
	constexpr uint32_t FirstControlGlyph = 0xE000;
	const auto& names = Strings::Current().controllerButtons;
	return glyph >= FirstControlGlyph && glyph - FirstControlGlyph < names.size()
		? names[glyph - FirstControlGlyph] : std::string_view{};
}
std::string Read(std::string_view text) {
	std::string result;
	for (size_t i = 0; i < text.size();) {
		uint32_t glyph = 0;
		if (GlyphAt(text, i, glyph)) {
			const auto button = Button(glyph);
			if (!button.empty()) { result += ' '; result += button; result += ' '; i += 3; continue; }
		}
		result += text[i++];
	}
	return UiText::Clean(result);
}
std::string_view FirstButton(std::string_view text) {
	for (size_t i = 0; i < text.size(); ++i) {
		uint32_t glyph = 0;
		if (!GlyphAt(text, i, glyph)) continue;
		if (const auto button = Button(glyph); !button.empty()) return button;
	}
	return {};
}
}
