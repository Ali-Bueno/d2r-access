#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace D2RAccess::GroundItemText {
std::string BaseName(std::string_view localized);
std::string Describe(std::string_view name, uint32_t quality, int goldAmount);
const char* Enabled(bool enabled);
const char* SoundEnabled(bool enabled);
}
