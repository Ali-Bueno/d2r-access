#include "monster_plate.h"

#include <vector>

namespace D2RAccess::MonsterPlate {
namespace {
// A gender tag: "[ms]", "[fs]", "[pl]" and the rest, two lower-case letters.
constexpr size_t TagSize = 4;
bool IsTag(std::string_view text, size_t at) {
	const auto lower = [](char c) { return c >= 'a' && c <= 'z'; };
	return at + TagSize <= text.size() && text[at] == '[' && lower(text[at + 1]) && lower(text[at + 2]) && text[at + 3] == ']';
}
struct Variant { std::string_view tag, text; };
std::vector<Variant> Variants(std::string_view text) {
	std::vector<Variant> variants;
	std::string_view tag;
	size_t start = 0;
	for (size_t at = 0; at <= text.size(); ++at) {
		if (at < text.size() && !IsTag(text, at)) continue;
		if (at > start) variants.push_back({tag, text.substr(start, at - start)});
		if (at == text.size()) break;
		tag = text.substr(at, TagSize);
		start = at + TagSize;
		at += TagSize - 1;
	}
	return variants;
}
std::string Trim(std::string text) {
	const size_t first = text.find_first_not_of(' ');
	if (first == std::string::npos) return {};
	return text.substr(first, text.find_last_not_of(' ') - first + 1);
}
// "a<i>n<j>:": the grammar header a format may open with.
constexpr size_t HeaderSize = 5;
bool Digit(char c) { return c >= '0' && c <= '9'; }
}

std::string Form(std::string_view text, std::string_view tag) {
	const auto variants = Variants(text);
	if (variants.empty()) return {};
	for (const auto& variant : variants)
		if (!tag.empty() && variant.tag == tag) return std::string(variant.text);
	return std::string(variants.front().text);
}

std::string Compose(std::string_view format, std::span<const std::string> parts) {
	size_t adjective = parts.size(), noun = parts.size();
	if (format.size() >= HeaderSize && format[0] == 'a' && Digit(format[1]) && format[2] == 'n' && Digit(format[3]) && format[4] == ':') {
		adjective = static_cast<size_t>(format[1] - '0');
		noun = static_cast<size_t>(format[3] - '0');
		format.remove_prefix(HeaderSize);
	}
	// INFERRED, not read from the game: a noun with no tag of its own takes the
	// masculine singular, which is how the untagged monster names read
	// ("Gefallener", "Caído").
	std::string_view gender = "[ms]";
	if (noun < parts.size() && IsTag(parts[noun], 0)) gender = std::string_view(parts[noun]).substr(0, TagSize);
	std::string result;
	for (size_t at = 0; at < format.size(); ++at) {
		if (format[at] == '%' && at + 1 < format.size() && Digit(format[at + 1])) {
			const size_t index = static_cast<size_t>(format[at + 1] - '0');
			if (index < parts.size()) result += Form(parts[index], index == adjective ? gender : std::string_view{});
			++at;
			continue;
		}
		result += format[at];
	}
	return Trim(std::move(result));
}
}
