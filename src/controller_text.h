#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace D2RAccess::ControllerText {
std::string_view Button(uint32_t glyph);
// Decode controller font symbols only in control help and binding labels.
std::string Read(std::string_view text);
// The control hints without the entry for one button ("View: View Advanced
// Stats"), where the mod has taken that button over. Entries are
// "<button>: <caption>", joined as NativeControllerHelp joins them.
std::string WithoutControl(std::string_view hints, std::string_view button);
}
