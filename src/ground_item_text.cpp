#include "ground_item_text.h"
#include "speech_strings.h"
#include <D2RLPlugin/item.h>
#include <algorithm>
#include <atomic>
#include <cstdio>

namespace D2RAccess::GroundItemText {
namespace {
// Grammatical variants in the installed local/lng/strings/item-names.json.
constexpr std::string_view GenderTags[]{"[fs]", "[ms]", "[fp]", "[mp]", "[ns]", "[pl]", "[nl]"};
// A name format may open with the grammar header "a<adjective arg>n<noun arg>:".
constexpr size_t HeaderSize = 5, NounDigit = 3;
std::atomic<bool> g_silver{true};
std::atomic<LoaderLabels> g_loaderLabels{};
constexpr std::string_view BaseArg = "%0", SuffixArg = "%1";

size_t FirstTag(std::string_view text) {
	size_t first = std::string_view::npos;
	for (auto tag : GenderTags) first = std::min(first, text.find(tag));
	return first;
}
}
std::string RepairSuffixOnlyName(std::string_view text, std::string_view format) {
	// The base is argument 0; a header naming 1 the noun makes the game print the
	// base, tag and all, as the suffix's modifier.
	if (format.size() < HeaderSize || format[0] != 'a' || format[NounDigit - 1] != 'n'
		|| format[NounDigit] != '1' || format[HeaderSize - 1] != ':') return std::string(text);
	const auto body = format.substr(HeaderSize);
	const size_t suffixAt = body.find(SuffixArg), baseAt = body.find(BaseArg);
	// Only a base printed after the suffix can be told from it, by its tag.
	if (suffixAt == std::string_view::npos || baseAt == std::string_view::npos || baseAt < suffixAt) return std::string(text);
	const auto before = body.substr(0, suffixAt);
	const auto between = body.substr(suffixAt + SuffixArg.size(), baseAt - suffixAt - SuffixArg.size());
	const auto after = body.substr(baseAt + BaseArg.size());
	const size_t tag = FirstTag(text);
	if (tag == std::string_view::npos || !text.starts_with(before) || !text.ends_with(after)
		|| tag <= before.size() + between.size() || tag + after.size() >= text.size()
		|| text.substr(tag - between.size(), between.size()) != between) return std::string(text);
	const auto suffix = text.substr(before.size(), tag - between.size() - before.size());
	const auto base = BaseName(text.substr(tag, text.size() - after.size() - tag));
	// The meant format swaps the two roles ("a1n0:%0 %1" in esMX).
	return std::string(before) + base + std::string(between) + std::string(suffix) + std::string(after);
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
bool IsSilver(uint32_t quality, uint32_t flags) {
	if (quality >= FirstColouredQuality && quality <= LastColouredQuality) return false;
	if (flags & (FlagBroken | FlagRuneword)) return false;
	return (flags & (FlagSocketed | FlagEthereal)) != 0;
}
bool SilverEnabled() { return g_silver.load(std::memory_order_relaxed); }
void SetSilverEnabled(bool enabled) { g_silver.store(enabled, std::memory_order_relaxed); }
LoaderLabels CurrentLoaderLabels() { return g_loaderLabels.load(std::memory_order_relaxed); }
void SetLoaderLabels(LoaderLabels labels) { g_loaderLabels.store(labels, std::memory_order_relaxed); }
bool LabelsSockets(uint32_t quality) {
	using D2RL::Items::Quality;
	return quality == static_cast<uint32_t>(Quality::Normal) || quality == static_cast<uint32_t>(Quality::Superior);
}
std::string Describe(std::string_view name, uint32_t quality, int goldAmount, uint32_t itemFlags, LabelNumbers numbers) {
	if (name.empty()) return {};
	// A pile's amount is said when it is picked up (GroundItems::Purse), not on every pass.
	if (goldAmount > 0) return std::string(name);
	std::string result(name);
	const auto& labels = Strings::Current().itemQualities;
	if (quality < labels.size() && *labels[quality]) { result += ", "; result += labels[quality]; }
	if (SilverEnabled() && IsSilver(quality, itemFlags)) { result += ", "; result += Strings::Current().silverItem; }
	if (numbers.level) { result += ", "; result += std::to_string(numbers.level); }
	if (numbers.sockets) {
		char text[80]{};
		std::snprintf(text, sizeof(text), Strings::Current().socketsCount, static_cast<int>(numbers.sockets));
		result += ", "; result += text;
	}
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
const char* DoorSoundEnabled(bool enabled) {
	const auto& t = Strings::Current();
	return enabled ? t.doorSoundsEnabled : t.doorSoundsDisabled;
}
const char* QuestSoundEnabled(bool enabled) {
	const auto& t = Strings::Current();
	return enabled ? t.questSoundsEnabled : t.questSoundsDisabled;
}
const char* StashSoundEnabled(bool enabled) {
	const auto& t = Strings::Current();
	return enabled ? t.stashSoundEnabled : t.stashSoundDisabled;
}
}
