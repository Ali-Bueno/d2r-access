#include "ground_item_text.h"
#include "speech_strings.h"
#include <D2RLPlugin/item.h>
#include <algorithm>

namespace D2RAccess::GroundItemText {
namespace {
// Grammatical variants in the installed local/lng/strings/item-names.json.
constexpr std::string_view GenderTags[]{"[fs]", "[ms]", "[fp]", "[mp]", "[ns]", "[pl]", "[nl]"};
}
std::string BaseName(std::string_view localized) {
	bool prefix = true;
	while (prefix) {
		prefix = false;
		for (auto tag : GenderTags) if (localized.starts_with(tag)) { localized.remove_prefix(tag.size()); prefix = true; break; }
	}
	// A bare item name needs one display form, not all grammatical alternatives.
	size_t end = localized.size();
	for (auto tag : GenderTags) end = std::min(end, localized.find(tag));
	return std::string(localized.substr(0, end));
}
std::string Describe(std::string_view name, uint32_t quality, int goldAmount) {
	if (name.empty()) return {};
	if (goldAmount > 0) return std::to_string(goldAmount) + " " + std::string(name);
	std::string result(name);
	const auto& labels = Strings::Current().itemQualities;
	if (quality < labels.size() && *labels[quality]) { result += ", "; result += labels[quality]; }
	return result;
}
const char* Enabled(bool enabled) {
	const auto& t = Strings::Current();
	return enabled ? t.itemSpeechEnabled : t.itemSpeechDisabled;
}
const char* SoundEnabled(bool enabled) {
	const auto& t = Strings::Current();
	return enabled ? t.itemSoundsEnabled : t.itemSoundsDisabled;
}
const char* ExitSoundEnabled(bool enabled) {
	const auto& t = Strings::Current();
	return enabled ? t.exitSoundsEnabled : t.exitSoundsDisabled;
}
const char* ChestSoundEnabled(bool enabled) {
	const auto& t = Strings::Current();
	return enabled ? t.chestSoundsEnabled : t.chestSoundsDisabled;
}
}
