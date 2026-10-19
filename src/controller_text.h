#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace D2RAccess::ControllerText {
// A control symbol's name: a physical control (U+E000..E012), or the button bound to a UI action
// (U+E013..E02A: select, back, pause...); empty for any other code.
std::string_view Button(uint32_t glyph);
// Decode controller font symbols only in control help and binding labels.
std::string Read(std::string_view text);
// The first control symbol's name, e.g. the button a help line is about.
std::string_view FirstButton(std::string_view text);
}
