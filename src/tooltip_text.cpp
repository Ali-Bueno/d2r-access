#include "tooltip_text.h"

#include "ui_text.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace D2RAccess::TooltipText {
namespace {

// U+E07E introduces the renderer's inline style command, and exactly one
// character follows it. The renderer accepts that character when `c - '0'` is
// below 0x27, so the index runs '0' through 'V' -- measured in the 3.3.93787
// image at 0x140908BD1, 0x14090C225 and 0x14090F660, which range-check it
// identically. Index 2 is the one a title is drawn in.
//
// Only '1' and '2' were accepted here before, so an item tooltip -- drawn with
// index '0' -- kept that '0' and spoke it at the head of every line.
constexpr std::string_view Font = "\xEE\x81\xBE";
constexpr std::string_view Heading = "\xEE\x81\xBE" "2";
// Item lines take D2's palette through that index: '0' white, '3' magic blue,
// '5' socketed grey, and '1' red, an unmet requirement (log, 2026-09-25 07:49).
constexpr std::string_view Red = "\xEE\x81\xBE" "1";

using UiText::FontIndex;

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

std::string Markup(std::string_view body) {
	constexpr size_t Preview = 24; // enough of the line to recognise it in the log
	std::string result;
	for (size_t index = 0;; ++index) {
		const size_t newline = body.find('\n');
		const std::string_view raw = body.substr(0, newline);
		std::string codes;
		for (size_t i = 0; i < raw.size(); ++i) {
			const auto byte = static_cast<unsigned char>(raw[i]);
			char code[16]{};
			if (raw.substr(i, 3) == "\xC3\xBF" "c" && i + 3 < raw.size()) {
				std::snprintf(code, sizeof code, "yc%c ", raw[i + 3]);
				i += 3;
			} else if (byte == 0xFF && i + 2 < raw.size() && raw[i + 1] == 'c') {
				std::snprintf(code, sizeof code, "yc%c ", raw[i + 2]);
				i += 2;
			} else if (byte >= 0xEE && byte <= 0xEF && i + 2 < raw.size()) {
				const unsigned point = ((byte & 0x0F) << 12) | ((raw[i + 1] & 0x3F) << 6) | (raw[i + 2] & 0x3F);
				if (point >= 0xE000 && point <= 0xF8FF) std::snprintf(code, sizeof code, "U+%04X ", point);
				i += 2;
				// The style command's index is part of it.
				if (raw.substr(i - 2, Font.size()) == Font && i + 1 < raw.size() && FontIndex(raw[i + 1]))
					std::snprintf(code, sizeof code, "U+%04X:%c ", point, raw[++i]);
			} else if (byte < 0x20) {
				std::snprintf(code, sizeof code, "\\x%02X ", byte);
			}
			codes += code;
		}
		if (!codes.empty()) {
			std::string text = UiText::Clean(StripFont(std::string(raw)));
			size_t cut = std::min(text.size(), Preview);
			while (cut < text.size() && cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;
			codes.pop_back();
			result += "L" + std::to_string(index) + "[" + codes + "] " + text.substr(0, cut) + " | ";
		}
		if (newline == std::string_view::npos) break;
		body.remove_prefix(newline + 1);
	}
	return result;
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
		const bool  red = raw.find(Red) != std::string::npos;
		std::string line = UiText::Clean(StripFont(std::move(raw)));
		if (!line.empty()) {
			// Reading upwards, the last styled line seen is the topmost one.
			if (heading) result.heading = result.lines.size();
			result.lines.push_back(std::move(line));
			result.red.push_back(red);
		}
		if (newline == std::string_view::npos) break;
		body = body.substr(0, newline);
	}
	return result;
}

std::string Comparison(std::span<const std::string> extras) {
	std::string result;
	for (const auto& extra : extras)
		for (const auto& line : Read(extra).lines) Append(result, line);
	return result;
}

Description Split(const Body& body, std::span<const std::string> statLabels,
	std::span<const std::string> priceLabels, std::string_view unidentifiedLabel,
	std::span<const std::string> requirementLabels, std::span<const std::string> quantityLabels) {
	Description result;
	if (body.lines.empty()) return result;
	const auto starts = [](const std::string& line, std::span<const std::string> labels) {
		return std::any_of(labels.begin(), labels.end(),
			[&](const std::string& prefix) { return StartsWith(line, prefix); });
	};
	const auto price = [&](const std::string& line) { return starts(line, priceLabels); };
	const auto unidentified = [&](const std::string& line) { return !unidentifiedLabel.empty() && line == unidentifiedLabel; };
	const auto quantity = [&](const std::string& line) { return starts(line, quantityLabels); };
	const auto labelled = [&](const std::string& line) { return starts(line, statLabels) || price(line) || unidentified(line) || quantity(line); };
	// The styled title where there is one. Otherwise the first line that is not
	// one of the labelled ones: a vendor puts its price where an inventory
	// tooltip has the name, and a price is not an item. When every line is
	// labelled there is nothing better than the first.
	size_t name = body.Marked() ? body.heading : 0;
	if (!body.Marked()) {
		for (size_t at = 0; at < body.lines.size(); ++at)
			if (!labelled(body.lines[at])) { name = at; break; }
	}
	result.name = body.lines[name];
	// A stack's quantity and a price join it, wherever they sit. Everything
	// else, the base damage or defense included, keeps the order the screen drew
	// it in and waits to be asked for.
	std::string& stack = result.stack;
	for (size_t at = 0; at < body.lines.size(); ++at) {
		if (unidentified(body.lines[at])) { result.unidentified = true; continue; }
		if (at == name) continue;
		if (price(body.lines[at])) Append(result.price, body.lines[at]);
		if (quantity(body.lines[at])) { Append(stack, body.lines[at]); continue; }
		if (price(body.lines[at])) continue;
		Append(result.detail, body.lines[at]);
		if (at < body.red.size() && body.red[at] && starts(body.lines[at], requirementLabels)) Append(result.unmet, body.lines[at]);
	}
	// Unidentified is the name's status, said once right after it.
	if (result.unidentified && result.name != unidentifiedLabel) Append(result.name, unidentifiedLabel);
	result.headline = Headline(result);
	return result;
}

std::string Headline(const Description& description) {
	std::string headline = description.name;
	Append(headline, description.stack);
	Append(headline, description.price);
	return headline;
}

}
