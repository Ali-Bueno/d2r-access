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

NativeUi::Cell SkillCell(std::string_view name) {
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
	return cell;
}

std::string SkillDescription(std::string_view body) {
	// U+E07E followed by 1 or 2 sets font size. Captured from the stock skill
	// renderer during controller navigation; these are not controller glyphs.
	constexpr std::string_view font = "\xEE\x81\xBE";
	constexpr std::string_view heading = "\xEE\x81\xBE" "2";
	std::vector<std::string> lines;
	size_t title = std::string::npos;
	while (!body.empty()) {
		const auto newline = body.rfind('\n');
		std::string line(body.substr(newline == std::string_view::npos ? 0 : newline + 1));
		const bool isTitle = title == std::string::npos && line.find(heading) != std::string::npos;
		for (size_t at = 0; (at = line.find(font, at)) != std::string::npos;) {
			const size_t next = at + font.size();
			const bool size = next < line.size() && (line[next] == '1' || line[next] == '2');
			line.erase(at, font.size() + (size ? 1 : 0));
		}
		line = UiText::Clean(line);
		if (!line.empty()) {
			if (isTitle) title = lines.size();
			lines.push_back(std::move(line));
		}
		if (newline == std::string_view::npos) break;
		body = body.substr(0, newline);
	}
	if (lines.empty()) return {};
	// The learned/unlearned state is drawn above the styled name. Speak the
	// name first, then preserve the rest of the screen's top-to-bottom order.
	if (title != std::string::npos) std::rotate(lines.begin(), lines.begin() + title, lines.begin() + title + 1);
	std::string result = lines.front();
	if (lines.size() > 1) result += ".";
	for (size_t i = 1; i < lines.size(); ++i) result += " " + lines[i];
	return result;
}
}
