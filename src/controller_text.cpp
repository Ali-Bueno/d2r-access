#include "controller_text.h"
#include "speech_strings.h"
#include "ui_text.h"

#include <array>

namespace D2RAccess::ControllerText {
namespace {
// From U+E013 the codes are UI actions drawn as the button bound to them (0x45BC0 maps code to
// action, 0x13CBB0 action to button); PC's bindings are fixed (0x13D0C0), so each is its button's
// code, 0 where nothing is bound. docs/controller-glyphs.md
constexpr uint32_t FirstActionGlyph = 0xE013;
constexpr std::array<uint32_t, 24> ActionButtons{
	0xE00F, 0xE010, 0xE011, 0xE012, // select A, back B, option X, option Y
	0xE008, 0xE00A, 0, 0,           // option L3, option R3, two unbound
	0xE00D, 0xE00E, 0xE005, 0xE006, // tab left LT, tab right RT, pause Menu, menu View
	0xE003, 0xE001, 0xE002, 0xE004, // belt 1-4: D-pad left, up, down, right
	0, 0, 0, 0,                     // navigating, on the stick
	0xE00B, 0xE00C, 0, 0};          // LB, RB, two unbound
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
	// The font's first 19 codes are physical controls (registered at 0x45E80; 0x872E50 maps the
	// legend's names to them). Xbox face labels match the live UI; 0x1441F0 confirms the XInput masks.
	if (glyph >= FirstActionGlyph && glyph - FirstActionGlyph < ActionButtons.size())
		glyph = ActionButtons[glyph - FirstActionGlyph];
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
