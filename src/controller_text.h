#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace D2RAccess::ControllerText {
std::string_view Button(uint32_t glyph);
// Decode controller font symbols only in control help and binding labels.
std::string Read(std::string_view text);
// The first control symbol's name, e.g. the button a help line is about.
std::string_view FirstButton(std::string_view text);
}
