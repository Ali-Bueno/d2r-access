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

std::string LabelPrefix(std::string_view entry) {
	// Up to the first printf placeholder, and no further than the colon that
	// ends the label where there is one -- "Defense: %s" once the colour
	// escapes in the range form are taken into account. Trailing space is
	// dropped so the prefix matches however the line spaces itself.
	size_t end = entry.find('%');
	if (const size_t colon = entry.find(':'); colon != std::string_view::npos && colon + 1 < end) end = colon + 1;
	if (end == std::string_view::npos) end = entry.size();
	while (end > 0 && entry[end - 1] == ' ') --end;
	return std::string(entry.substr(0, end));
}

Body Read(std::string_view body) {
	// The engine stores a body bottom-up: its first line is the bottom of the
	// drawn tooltip and its last is the title. Walk it backwards so the lines
	// come out in the order the screen draws them, the item's name first.
	Body result;
	while (!body.empty()) {
		const auto  newline = body.rfind('\n');
		std::string raw(body.substr(newline == std::string_view::npos ? 0 : newline + 1));
		const bool  heading = raw.find(Heading) != std::string::npos;
		std::string line = UiText::Clean(StripFont(std::move(raw)));
		if (!line.empty()) {
			// Reading upwards, the last styled line seen is the topmost one.
			if (heading) result.heading = result.lines.size();
			result.lines.push_back(std::move(line));
		}
		if (newline == std::string_view::npos) break;
		body = body.substr(0, newline);
	}
	return result;
}

Description Split(const Body& body, std::span<const std::string> statLabels,
	std::span<const std::string> priceLabels) {
	Description result;
	if (body.lines.empty()) return result;
	const auto starts = [](const std::string& line, std::span<const std::string> labels) {
		return std::any_of(labels.begin(), labels.end(),
			[&](const std::string& prefix) { return StartsWith(line, prefix); });
	};
	const auto price = [&](const std::string& line) { return starts(line, priceLabels); };
	const auto labelled = [&](const std::string& line) { return starts(line, statLabels) || price(line); };
	// The styled title where there is one. Otherwise the first line that is not
	// one of the labelled ones: a vendor puts its price where an inventory
	// tooltip has the name, and a price is not an item. When every line is
	// labelled there is nothing better than the first.
	size_t name = body.Marked() ? body.heading : 0;
	if (!body.Marked()) {
		for (size_t at = 0; at < body.lines.size(); ++at)
			if (!labelled(body.lines[at])) { name = at; break; }
	}
	result.name = result.headline = body.lines[name];
	// A price joins it, wherever it sits. Everything else, the base damage or
	// defense included, keeps the order the screen drew it in and waits to be
	// asked for.
	for (size_t at = 0; at < body.lines.size(); ++at) {
		if (at == name) continue;
		if (price(body.lines[at])) Append(result.price, body.lines[at]);
		Append(price(body.lines[at]) ? result.headline : result.detail, body.lines[at]);
	}
	return result;
}

}
