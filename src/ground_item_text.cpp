#include "ground_item_text.h"
#include "speech_strings.h"
#include <D2RLPlugin/item.h>
#include <array>
#include <algorithm>

namespace D2RAccess::GroundItemText {
namespace {
// Plugin connective text lives here; item names always come from localization.
bool Spanish() { return &Strings::Current() == &Strings::Spanish; }
constexpr std::array<const char*, 9> QualityEnglish{
	"", "low quality", "", "superior", "magic", "set", "rare", "unique", "crafted"};
constexpr std::array<const char*, 9> QualitySpanish{
	"", "calidad inferior", "", "superior", "mágico", "de conjunto", "raro", "único", "fabricado"};
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
	const auto& labels = Spanish() ? QualitySpanish : QualityEnglish;
	if (quality < labels.size() && *labels[quality]) { result += ", "; result += labels[quality]; }
	return result;
}
const char* Enabled(bool enabled) {
	return Spanish() ? (enabled ? "Lectura de objetos cercanos activada" : "Lectura de objetos cercanos desactivada")
		: (enabled ? "Nearby item speech enabled" : "Nearby item speech disabled");
}
const char* SoundEnabled(bool enabled) {
	return Spanish() ? (enabled ? "Sonidos de objetos activados" : "Sonidos de objetos desactivados")
		: (enabled ? "Item sounds enabled" : "Item sounds disabled");
}
}
