#include "quest_log.h"

#include <cstring>
#include <string_view>

namespace D2RAccess::QuestLog {
namespace {

// The conversions D2's quest lines actually use for their live counters.
constexpr std::string_view Conversions[] = {"%d", "%i", "%u"};

// Where the first runtime value sits in a string-table line, and how long the
// conversion is; count 0 when the line carries none. "%%" is a literal percent
// sign and never a value.
struct Placeholder {
	size_t at = std::string_view::npos;
	size_t count = 0;
};

Placeholder ValueAt(std::string_view text) {
	for (size_t i = 0; i + 1 < text.size(); ++i) {
		if (text[i] != '%') continue;
		if (text[i + 1] == '%') { ++i; continue; }
		for (std::string_view conversion : Conversions) {
			if (text.compare(i, conversion.size(), conversion) == 0) return {i, conversion.size()};
		}
	}
	return {};
}

std::string Resolved(const Resolver& resolve, const char* key) {
	return key && resolve ? resolve(key) : std::string();
}

}

const Generated::Quest* ForWidget(std::string_view widget) {
	if (widget.empty()) return nullptr;
	for (const auto& quest : Generated::Quests) {
		if (widget == quest.widget) return &quest;
	}
	return nullptr;
}

std::string Title(const Generated::Quest& quest, const Resolver& resolve) {
	return Resolved(resolve, quest.titleKey);
}

bool NeedsValue(std::string_view text) {
	return ValueAt(text).count != 0;
}

std::vector<std::string> Objectives(const Generated::Quest& quest, const Resolver& resolve) {
	std::vector<std::string> result;
	for (uint8_t i = 0; i < quest.stepCount; ++i) {
		auto text = Resolved(resolve, quest.stepKeys[i]);
		if (text.empty() || NeedsValue(text)) continue;
		result.push_back(std::move(text));
	}
	return result;
}

std::string Counter(const Generated::Quest& quest, int remaining, const Resolver& resolve) {
	for (uint8_t i = 0; i < quest.stepCount; ++i) {
		const char* key = quest.stepKeys[i];
		auto text = Resolved(resolve, key);
		const Placeholder value = ValueAt(text);
		if (!value.count) continue;
		// The game ships the last-one-remaining wording as its own line, keyed
		// by the counter's key with a trailing zero ("qstsa1q14" -> "qstsa1q140").
		if (remaining == 1) {
			const std::string singularKey = std::string(key) + "0";
			for (uint8_t j = 0; j < quest.stepCount; ++j) {
				if (singularKey != quest.stepKeys[j]) continue;
				auto singular = Resolved(resolve, quest.stepKeys[j]);
				if (!singular.empty()) return singular;
			}
		}
		// Only the conversion itself is replaced; the rest of the game's
		// sentence, punctuation included, is left exactly as shipped.
		text.replace(value.at, value.count, std::to_string(remaining));
		return text;
	}
	return {};
}

}
