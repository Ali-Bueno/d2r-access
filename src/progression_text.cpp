#include "progression_text.h"

#include <algorithm>
#include <charconv>
#include <vector>

namespace D2RAccess::ProgressionText {
TooltipText::Description SkillReadout(std::string_view description, std::string_view points, std::string_view help) {
	const auto append = [](std::string& text, std::string_view part) {
		if (part.empty()) return;
		if (!text.empty()) text += text.ends_with('.') ? " " : ". ";
		text += part;
	};
	// SkillDescription puts the skill's name first and ends it with a full
	// stop; everything past that is what the skill does and what it costs.
	const auto split = description.find(". ");
	TooltipText::Description result;
	result.headline = std::string(description.substr(0, split));
	append(result.headline, points);
	append(result.headline, help);
	if (split != std::string_view::npos) append(result.detail, description.substr(split + 2));
	return result;
}

SkillButton ParseSkillButton(std::string_view name) {
	constexpr std::string_view prefix = "SkillButton"; // live generated names, docs/progression.md
	if (!name.starts_with(prefix)) return {};
	name.remove_prefix(prefix.size());
	int tree = 0;
	const char* end = name.data() + name.size();
	const auto group = std::from_chars(name.data(), end, tree);
	if (group.ec != std::errc{} || group.ptr == end || *group.ptr != ' ' || tree < 0) return {};
	NativeUi::Cell cell;
	const auto column = std::from_chars(group.ptr + 1, end, cell.x);
	if (column.ec != std::errc{} || column.ptr == end || *column.ptr != ',') return {};
	const auto row = std::from_chars(column.ptr + 1, end, cell.y);
	// The bound is the shared native tree traversal limit, a corruption guard.
	constexpr int MaxCoordinate = 512;
	if (row.ec != std::errc{} || row.ptr != end || cell.x < 0 || cell.y < 0
		|| cell.x >= MaxCoordinate || cell.y >= MaxCoordinate) return {};
	return {tree, cell};
}

NativeUi::Cell SkillCell(std::string_view name) { return ParseSkillButton(name).cell; }

namespace {
bool EndsSentence(std::string_view line) {
	return line.ends_with('.') || line.ends_with(':') || line.ends_with('!') || line.ends_with('?');
}

// Whether a line begins with a lowercase letter: ASCII, Latin-1, Latin
// Extended-A (lowercase on odd code points) and Cyrillic, the game's scripts with case.
bool StartsLowercase(std::string_view line) {
	if (line.empty()) return false;
	const auto a = static_cast<unsigned char>(line[0]);
	if (a < 0x80) return a >= 'a' && a <= 'z';
	if (line.size() < 2) return false;
	const auto b = static_cast<unsigned char>(line[1]);
	const unsigned point = ((a & 0x1F) << 6) | (b & 0x3F);
	if ((a & 0xE0) != 0xC0) return false;
	if (point >= 0xDF && point <= 0xFF) return point != 0xF7;           // ß to ÿ, not ÷
	if (point >= 0x100 && point <= 0x17F) return (point & 1) != 0;      // ā, ą, ł...
	return point >= 0x430 && point <= 0x45F; // а-я, ѐ-џ
}
}

std::vector<std::string> SkillLines(std::string_view body) {
	// U+E07E followed by 1 or 2 sets font size. Captured from the stock skill
	// renderer during controller navigation; these are not controller glyphs.
	constexpr std::string_view font = "\xEE\x81\xBE";
	constexpr std::string_view heading = "\xEE\x81\xBE" "2";
	std::vector<std::string> lines;
	size_t title = std::string::npos;
	while (!body.empty()) {
		const auto newline = body.rfind('\n');
		const std::string line(body.substr(newline == std::string_view::npos ? 0 : newline + 1));
		const bool isTitle = title == std::string::npos && line.find(heading) != std::string::npos;
		// A style command after text starts a new block on the same line (the
		// synergies after "Mana Cost: 4", a tester): the line is cut there.
		std::vector<std::string> parts{std::string{}};
		for (size_t at = 0; at < line.size();) {
			if (line.compare(at, font.size(), font) != 0) { parts.back() += line[at++]; continue; }
			const size_t next = at + font.size();
			const bool size = next < line.size() && (line[next] == '1' || line[next] == '2');
			at = next + (size ? 1 : 0);
			if (!UiText::Clean(parts.back()).empty()) parts.emplace_back();
		}
		bool first = true;
		for (auto& part : parts) {
			part = UiText::Clean(part);
			if (part.empty()) continue;
			if (isTitle && first) title = lines.size();
			first = false;
			lines.push_back(std::move(part));
		}
		if (newline == std::string_view::npos) break;
		body = body.substr(0, newline);
	}
	// The learned/unlearned state is drawn above the styled name. Speak the
	// name first, then preserve the rest of the screen's top-to-bottom order.
	if (title != std::string::npos) std::rotate(lines.begin(), lines.begin() + title, lines.begin() + title + 1);
	// A sentence the tooltip wrapped is one line again ("Magically splits one
	// Arrow" / "or bolt into many."), so it is read without a pause in the middle.
	std::vector<std::string> joined;
	for (auto& line : lines) {
		if (joined.size() > 1 && !EndsSentence(joined.back()) && StartsLowercase(line)) joined.back() += " " + line;
		else joined.push_back(std::move(line));
	}
	return joined;
}

std::string SkillDescription(std::string_view body) {
	const auto lines = SkillLines(body);
	if (lines.empty()) return {};
	return JoinLines(lines);
}

std::string JoinLines(const std::vector<std::string>& lines) {
	std::string result;
	for (const auto& line : lines) {
		if (line.empty()) continue;
		// The same pause an item tooltip's lines get: a full stop unless the line has its own.
		if (!result.empty()) result += EndsSentence(result) ? " " : ". ";
		result += line;
	}
	return result;
}
}
