#include "controller_text.h"
#include "speech_strings.h"
#include "ui_text.h"

namespace D2RAccess::ControllerText {
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
		if (i + 2 < text.size()) {
			const auto a = static_cast<unsigned char>(text[i]);
			const auto b = static_cast<unsigned char>(text[i + 1]);
			const auto c = static_cast<unsigned char>(text[i + 2]);
			if (a == 0xEE && (b & 0xC0) == 0x80 && (c & 0xC0) == 0x80) {
				const auto button = Button(((a & 0x0F) << 12) | ((b & 0x3F) << 6) | (c & 0x3F));
				if (!button.empty()) { result += ' '; result += button; result += ' '; i += 3; continue; }
			}
		}
		result += text[i++];
	}
	return UiText::Clean(result);
}
std::string WithoutControl(std::string_view hints, std::string_view button) {
	const std::string prefix = std::string(button) + ": ";
	std::string result;
	for (size_t at = 0; at <= hints.size();) {
		const size_t end = hints.find(". ", at);
		const auto entry = hints.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
		if (!entry.starts_with(prefix) && !entry.empty()) {
			if (!result.empty()) result += ". ";
			result += entry;
		}
		if (end == std::string_view::npos) break;
		at = end + 2;
	}
	return result;
}
}
