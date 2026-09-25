#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace D2RAccess::GroundItemText {
std::string BaseName(std::string_view localized);
// The format of a magic name with a suffix and no prefix (local/lng/strings/ui.json).
inline constexpr const char* SuffixOnlyFormatKey = "ItemNameMagicFormatSuffixOnly";
// A name the game built with a suffix-only format that takes the suffix for the
// noun ("de llamas [fs]Maza" in esES), rebuilt as meant ("Maza de llamas").
// Any other text comes back unchanged. docs/native-tooltips.md
std::string RepairSuffixOnlyName(std::string_view text, std::string_view suffixOnlyFormat);
std::string Describe(std::string_view name, uint32_t quality, int goldAmount);
const char* Enabled(bool enabled);
const char* SoundEnabled(bool enabled);
const char* ExitSoundEnabled(bool enabled);
const char* ChestSoundEnabled(bool enabled);
}
