#include "tooltip_text.h"

#include "ui_text.h"

#include <algorithm>
#include <cctype>

namespace D2RAccess::TooltipText {
namespace {

// U+E07E introduces the renderer's inline font command, and exactly one
// character follows it. The renderer accepts that character when `c - '0'` is
// below 0x27, so the index runs '0' through 'V' -- measured in the 3.3.93787
// image at 0x140908BD1, 0x14090C225 and 0x14090F660, which range-check it
// identically. Index 2 is the one a title is drawn in.
//
// Only '1' and '2' were accepted here before, so an item tooltip -- drawn with
// index '0' -- kept that '0' and spoke it at the head of every line.
constexpr std::string_view Font = "\xEE\x81\xBE";
constexpr std::string_view Heading = "\xEE\x81\xBE" "2";

bool FontIndex(char c) {
	return static_cast<unsigned>(static_cast<unsigned char>(c) - '0') < 0x27u;
}

std::string StripFont(std::string line) {
	for (size_t at = 0; (at = line.find(Font, at)) != std::string::npos;) {
		const size_t next = at + Font.size();
		const bool   index = next < line.size() && FontIndex(line[next]);
		line.erase(at, Font.size() + (index ? 1 : 0));
	}
	return line;
}

bool StartsWith(std::string_view line, std::string_view prefix) {
	if (prefix.empty() || line.size() < prefix.size()) return false;
	return std::equal(prefix.begin(), prefix.end(), line.begin(), [](unsigned char a, unsigned char b) {
		return std::tolower(a) == std::tolower(b);
	});
}

void Append(std::string& text, std::string_view part) {
	if (part.empty()) return;
	if (!text.empty()) text += text.ends_with('.') ? " " : ". ";
	text += part;
}

}

Body Read(std::string_view body) {
	Body result;
	while (!body.empty()) {
		const auto  newline = body.find('\n');
		std::string raw(body.substr(0, newline));
		const bool  heading = !result.Marked() && raw.find(Heading) != std::string::npos;
		std::string line = UiText::Clean(StripFont(std::move(raw)));
		if (!line.empty()) {
			if (heading) result.heading = result.lines.size();
			result.lines.push_back(std::move(line));
		}
		if (newline == std::string_view::npos) break;
		body = body.substr(newline + 1);
	}
	return result;
}

Description Split(const Body& body, std::span<const std::string> baseStats) {
	Description result;
	if (body.lines.empty()) return result;
	// The styled title wherever the engine put it; the first line when it
	// styled nothing. Either way the name leads.
	const size_t name = body.Marked() ? body.heading : 0;
	result.headline = body.lines[name];
	// The base damage or defense joins it, wherever it sits. Everything else
	// keeps the order it was stored in and waits to be asked for.
	for (size_t at = 0; at < body.lines.size(); ++at) {
		if (at == name) continue;
		const bool base = std::any_of(baseStats.begin(), baseStats.end(),
			[&](const std::string& prefix) { return StartsWith(body.lines[at], prefix); });
		Append(base ? result.headline : result.detail, body.lines[at]);
	}
	return result;
}

}
