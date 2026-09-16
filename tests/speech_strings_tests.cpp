#include "speech_strings.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace D2RAccess;

namespace {

// The printf conversions of a phrase, in order ("%%" is a literal percent).
std::vector<std::string> Specifiers(const char* phrase) {
	std::vector<std::string> found;
	const std::string text(phrase);
	for (size_t i = 0; i < text.size(); ++i) {
		if (text[i] != '%') continue;
		if (i + 1 < text.size() && text[i + 1] == '%') { ++i; continue; }
		size_t end = i + 1;
		while (end < text.size() && std::string("sduf.0123456789").find(text[end]) != std::string::npos) ++end;
		found.push_back(text.substr(i, end - i));
		i = end - 1;
	}
	return found;
}

std::vector<const char*> Phrases(const Strings::Table& table) {
	std::vector<const char*> phrases;
	Strings::ForEachPhrase(table, [&](const char* s) { phrases.push_back(s); });
	return phrases;
}

}

int main() {
	using Strings::Language;
	const auto english = Phrases(Strings::For(Language::enUS));
	Check(!english.empty(), "the visitor enumerates phrases");

	for (size_t i = 0; i < Strings::LanguageCount; ++i) {
		const auto language = static_cast<Language>(i);
		const std::string code(Strings::LocaleCode(language));
		Check(Strings::FromLocale(code) == language, code + ": locale code round-trips");
		Check(Strings::FromProbeValue(Strings::TextLocaleProbe.values[i]) == language, code + ": probe value identifies the language");
		for (size_t j = 0; j < i; ++j) {
			Check(Strings::TextLocaleProbe.values[i] != Strings::TextLocaleProbe.values[j], code + ": probe value is unique");
		}

		const auto& table = Strings::For(language);
		Check(Strings::Complete(table), code + ": every phrase is set");
		const auto phrases = Phrases(table);
		Check(phrases.size() == english.size(), code + ": same phrase count as English");
		for (size_t p = 0; p < phrases.size(); ++p) {
			const std::string where = code + " phrase " + std::to_string(p) + " \"" + phrases[p] + "\"";
			Check(Specifiers(phrases[p]) == Specifiers(english[p]), where + ": printf specifiers match English");
			Check((*english[p] == '\0') == (*phrases[p] == '\0'), where + ": empty exactly where English is empty");
			const std::string text(phrases[p]);
			Check(text.find('\n') == std::string::npos && text.find("\xC3\xBF" "c") == std::string::npos, where + ": no line breaks or colour codes");
		}

		Strings::Select(language);
		Check(Strings::Selected() == language && &Strings::Current() == &table, code + ": selection is observable");
	}

	Check(!Strings::FromLocale("xxXX").has_value(), "unknown locale code is rejected");
	Check(!Strings::FromProbeValue("CANCELAR").has_value(), "unknown probe value is rejected");
	Check(std::string(Strings::TextLocaleProbe.values[0]) == Strings::TextLocaleProbe.key, "the English probe value is the key itself (ui.json id 4143)");
	Passed();
	return 0;
}
