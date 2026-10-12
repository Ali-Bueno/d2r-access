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
// Item data flags (ItemData + 0x18), as the name-colour function 0x2BD360 tests them.
inline constexpr uint32_t FlagBroken = 0x100, FlagSocketed = 0x800, FlagEthereal = 0x400000, FlagRuneword = 0x4000000;
// Qualities 4..9 (magic .. last) pick their own colour in 0x2BD360; every other quality is a white name.
inline constexpr uint32_t FirstColouredQuality = 4, LastColouredQuality = 9;
// True when the game draws this item's name gray: a white-name quality that is socketed or
// ethereal, not broken (red) and not a runeword (named by its own text). docs/silver-items.md
bool IsSilver(uint32_t quality, uint32_t flags);
// The `silver-items` switch of the Ctrl+S list.
bool SilverEnabled();
void SetSilverEnabled(bool enabled);
std::string Describe(std::string_view name, uint32_t quality, int goldAmount, uint32_t itemFlags = 0);
const char* Enabled(bool enabled);
const char* SoundEnabled(bool enabled);
const char* ExitSoundEnabled(bool enabled);
const char* ChestSoundEnabled(bool enabled);
const char* DoorSoundEnabled(bool enabled);
const char* QuestSoundEnabled(bool enabled);
const char* StashSoundEnabled(bool enabled);
}
