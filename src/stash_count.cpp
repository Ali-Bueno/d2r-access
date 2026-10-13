#include "stash_count.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <D2RLPlugin/item.h>

namespace D2RAccess::StashCount {
std::string WithCount(const std::string& name, const std::string& shownText, bool shown, const char* separator) {
	if (!shown) return name; // an item whose count is not drawn yet; an empty slot has no tooltip
	const bool number = !shownText.empty()
		&& std::all_of(shownText.begin(), shownText.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
	return number ? name + separator + shownText : name;
}

namespace {
bool SameText(const std::string& a, const std::string& b) {
	return !a.empty() && std::equal(a.begin(), a.end(), b.begin(), b.end(), [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
}
}

GoldTab TabOf(const std::vector<std::string>& selected, const std::string& personal, const std::string& shared) {
	for (const auto& label : selected) {
		if (SameText(label, personal)) return GoldTab::Personal;
		if (SameText(label, shared)) return GoldTab::Shared;
	}
	return GoldTab::Unknown;
}

std::string Empty(const std::string& name, const char* emptyFormat) {
	char text[160]{};
	std::snprintf(text, sizeof(text), emptyFormat, name.c_str());
	return text;
}

uint32_t CodeOfWidgetName(std::string_view name) {
	constexpr size_t MaxCodeLength = 4; // item table codes are four bytes, space padded (D2RLPlugin/item.h)
	if (name.empty() || name.size() > MaxCodeLength
		|| !std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) != 0; })) return 0;
	char code[MaxCodeLength + 1]{' ', ' ', ' ', ' '};
	name.copy(code, name.size());
	return D2RL::Items::MakeItemCode(code[0], code[1], code[2], code[3]);
}
}
