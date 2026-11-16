#include "quest_log.h"

#include <cstring>
#include <optional>
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

namespace {

std::string_view Trimmed(std::string_view text) {
	constexpr std::string_view blanks = " \t\r";
	const auto first = text.find_first_not_of(blanks);
	if (first == std::string_view::npos) return {};
	return text.substr(first, text.find_last_not_of(blanks) - first + 1);
}

// The digits between the template's prefix and suffix, when the whole line is that template.
std::optional<int> FilledValue(std::string_view line, std::string_view pattern, Placeholder value) {
	const auto prefix = pattern.substr(0, value.at), suffix = pattern.substr(value.at + value.count);
	if (line.size() <= prefix.size() + suffix.size() || !line.starts_with(prefix) || !line.ends_with(suffix)) return std::nullopt;
	const auto digits = line.substr(prefix.size(), line.size() - prefix.size() - suffix.size());
	constexpr size_t MaxDigits = 6; // a monster count never nears this; it keeps int safe
	if (digits.size() > MaxDigits) return std::nullopt;
	int number = 0;
	for (const char c : digits) {
		if (c < '0' || c > '9') return std::nullopt;
		number = number * 10 + (c - '0');
	}
	return number;
}

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

std::optional<int> CounterValue(const Generated::Quest& quest, std::string_view text, const Resolver& resolve) {
	for (uint8_t i = 0; i < quest.stepCount; ++i) {
		const char* key = quest.stepKeys[i];
		const auto pattern = Resolved(resolve, key);
		const Placeholder value = ValueAt(pattern);
		if (!value.count) continue;
		const std::string singularKey = std::string(key) + "0";
		std::string singular;
		for (uint8_t j = 0; j < quest.stepCount; ++j)
			if (singularKey == quest.stepKeys[j]) singular = Resolved(resolve, quest.stepKeys[j]);
		while (!text.empty()) {
			const auto end = text.find('\n');
			const auto line = Trimmed(text.substr(0, end));
			if (!singular.empty() && line == Trimmed(singular)) return 1;
			if (const auto number = FilledValue(line, Trimmed(pattern), value)) return number;
			if (end == std::string_view::npos) break;
			text.remove_prefix(end + 1);
		}
		return std::nullopt;
	}
	return std::nullopt;
}

}
