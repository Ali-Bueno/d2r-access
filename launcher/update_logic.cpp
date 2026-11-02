#include "update_logic.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cwctype>

namespace D2RAccess::Launcher::Update {

namespace {

constexpr std::string_view TestPrefix = "test-";
constexpr std::size_t      Sha256Hex  = 64;
constexpr int              HexBase    = 16;
constexpr int              TagYearDigits = 4, TagPairDigits = 2;
constexpr int              MaxDay = 31, MonthsInYear = 12;
constexpr std::size_t      MaxVersionDigits = 4;
constexpr unsigned         HighSurrogateFirst = 0xD800, LowSurrogateFirst = 0xDC00, LowSurrogateLast = 0xDFFF;
constexpr unsigned         AstralBase = 0x10000;

constexpr std::wstring_view PluginFolder  = L"d2rloader/plugins/d2r-access/";
constexpr std::wstring_view PlayerFiles[] = {L"settings.txt", L"tutorial.txt"};
constexpr std::wstring_view PlayerFolder  = L"explored/";
constexpr std::wstring_view TomlExtension = L".toml";

bool ReadNumber(std::string_view& text, int digits, int& out) {
	if (text.size() < static_cast<std::size_t>(digits)) return false;
	const auto [end, error] = std::from_chars(text.data(), text.data() + digits, out);
	if (error != std::errc{} || end != text.data() + digits) return false;
	text.remove_prefix(static_cast<std::size_t>(digits));
	return true;
}

bool Eat(std::string_view& text, char c) {
	if (text.empty() || text.front() != c) return false;
	text.remove_prefix(1);
	return true;
}

// One part of a numbered tag: up to MaxVersionDigits digits, no leading zero.
bool ReadVersionPart(std::string_view& text, int& out) {
	std::size_t digits = 0;
	while (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits]))) ++digits;
	if (digits == 0 || digits > MaxVersionDigits || (digits > 1 && text.front() == '0')) return false;
	std::from_chars(text.data(), text.data() + digits, out);
	text.remove_prefix(digits);
	return true;
}

// `MAJOR.MINOR[.PATCH]`.
std::optional<ReleaseTag> ParseNumberedTag(std::string_view rest) {
	ReleaseTag parsed;
	parsed.numbered = true;
	if (!ReadVersionPart(rest, parsed.versionMajor) || !Eat(rest, '.') || !ReadVersionPart(rest, parsed.versionMinor)) return std::nullopt;
	if (Eat(rest, '.') && !ReadVersionPart(rest, parsed.versionPatch)) return std::nullopt;
	if (!rest.empty()) return std::nullopt;
	return parsed;
}

void AppendUtf8(std::string& out, unsigned code) {
	const auto tail = [&](unsigned shift) { out += static_cast<char>(0x80 | ((code >> shift) & 0x3F)); };
	if (code < 0x80) out += static_cast<char>(code);
	else if (code < 0x800) { out += static_cast<char>(0xC0 | (code >> 6)); tail(0); }
	else if (code < AstralBase) { out += static_cast<char>(0xE0 | (code >> 12)); tail(6); tail(0); }
	else { out += static_cast<char>(0xF0 | (code >> 18)); tail(12); tail(6); tail(0); }
}

std::optional<unsigned> Hex4(std::string_view text) {
	if (text.size() < 4) return std::nullopt;
	unsigned value          = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + 4, value, HexBase);
	if (error != std::errc{} || end != text.data() + 4) return std::nullopt;
	return value;
}

// Reads a JSON string whose opening quote is `text.front()`; advances past the closing quote.
std::optional<std::string> ReadString(std::string_view& text) {
	if (!Eat(text, '"')) return std::nullopt;
	std::string out;
	while (!text.empty()) {
		const char c = text.front();
		text.remove_prefix(1);
		if (c == '"') return out;
		if (c != '\\') { out += c; continue; }
		if (text.empty()) return std::nullopt;
		const char escape = text.front();
		text.remove_prefix(1);
		switch (escape) {
		case '"': case '\\': case '/': out += escape; break;
		case 'b': out += '\b'; break;
		case 'f': out += '\f'; break;
		case 'n': out += '\n'; break;
		case 'r': out += '\r'; break;
		case 't': out += '\t'; break;
		case 'u': {
			auto code = Hex4(text);
			if (!code) return std::nullopt;
			text.remove_prefix(4);
			if (*code >= HighSurrogateFirst && *code < LowSurrogateFirst) {
				if (!text.starts_with("\\u")) return std::nullopt;
				const auto low = Hex4(text.substr(2));
				if (!low || *low < LowSurrogateFirst || *low > LowSurrogateLast) return std::nullopt;
				text.remove_prefix(6);
				code = AstralBase + ((*code - HighSurrogateFirst) << 10) + (*low - LowSurrogateFirst);
			}
			AppendUtf8(out, *code);
			break;
		}
		default: return std::nullopt;
		}
	}
	return std::nullopt;
}

void SkipSpace(std::string_view& text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
}

// Skips a value that is not a string: a literal, or a nested object or array.
bool SkipValue(std::string_view& text) {
	int depth = 0;
	while (!text.empty()) {
		const char c = text.front();
		if (c == '"') {
			if (!ReadString(text)) return false;
			continue;
		}
		if (depth == 0 && (c == ',' || c == '}')) return true;
		if (c == '{' || c == '[') ++depth;
		if (c == '}' || c == ']') --depth;
		text.remove_prefix(1);
	}
	return false;
}

std::string_view Trim(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
	return text;
}

}

std::optional<ReleaseTag> ParseReleaseTag(std::string_view tag) {
	ReleaseTag       parsed;
	std::string_view rest = tag;
	char             separator;
	if (!rest.empty() && std::isdigit(static_cast<unsigned char>(rest.front()))) return ParseNumberedTag(rest);
	if (rest.starts_with(TestPrefix)) {
		rest.remove_prefix(TestPrefix.size());
		separator = '-';
	} else if (Eat(rest, 'v')) {
		separator = '.';
	} else {
		return std::nullopt;
	}
	if (!ReadNumber(rest, TagYearDigits, parsed.year) || !Eat(rest, separator) || !ReadNumber(rest, TagPairDigits, parsed.month) ||
		!Eat(rest, separator) || !ReadNumber(rest, TagPairDigits, parsed.day)) return std::nullopt;
	if (Eat(rest, '-')) {
		const auto [end, error] = std::from_chars(rest.data(), rest.data() + rest.size(), parsed.number);
		if (error != std::errc{} || end != rest.data() + rest.size() || parsed.number < 1) return std::nullopt;
		rest = {};
	}
	if (!rest.empty() || parsed.month < 1 || parsed.month > MonthsInYear || parsed.day < 1 || parsed.day > MaxDay) return std::nullopt;
	return parsed;
}

bool IsNewer(const ReleaseTag& latest, const ReleaseTag& installed) { return latest > installed; }

std::optional<std::string> ExtractJsonString(std::string_view json, std::string_view key) {
	SkipSpace(json);
	if (!Eat(json, '{')) return std::nullopt;
	for (;;) {
		SkipSpace(json);
		const auto name = ReadString(json);
		SkipSpace(json);
		if (!name || !Eat(json, ':')) return std::nullopt;
		SkipSpace(json);
		if (!json.empty() && json.front() == '"') {
			const auto value = ReadString(json);
			if (!value) return std::nullopt;
			if (*name == key) return value;
		} else if (!SkipValue(json)) {
			return std::nullopt;
		}
		SkipSpace(json);
		if (!Eat(json, ',')) return std::nullopt;
	}
}

std::optional<Manifest> ParseManifest(std::string_view json) {
	const auto tag    = ExtractJsonString(json, "tag");
	const auto asset  = ExtractJsonString(json, "asset");
	const auto sha256 = ExtractJsonString(json, "sha256");
	if (!tag || !asset || !sha256 || !ParseReleaseTag(*tag) || !IsPlainFileName(*asset)) return std::nullopt;
	if (sha256->size() != Sha256Hex || !std::ranges::all_of(*sha256, [](unsigned char c) { return std::isxdigit(c) != 0; })) return std::nullopt;
	return Manifest{*tag, *asset, *sha256, std::string(Trim(ExtractJsonString(json, "notes").value_or("")))};
}

bool SwitchOn(std::string_view text, std::string_view key) {
	while (!text.empty()) {
		const std::size_t      end  = text.find('\n');
		const std::string_view line = Trim(text.substr(0, end));
		text                        = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
		const std::size_t eq        = line.find('=');
		if (eq == std::string_view::npos || Trim(line.substr(0, eq)) != key) continue;
		return Trim(line.substr(eq + 1)) != "0";
	}
	return true;
}

bool IsProtectedPath(std::wstring_view path) {
	std::wstring normal(path);
	for (wchar_t& c : normal) c = c == L'\\' ? L'/' : static_cast<wchar_t>(std::towlower(c));
	if (normal.ends_with(TomlExtension)) return true;
	if (!normal.starts_with(PluginFolder)) return false;
	const std::wstring_view inside = std::wstring_view(normal).substr(PluginFolder.size());
	return inside.starts_with(PlayerFolder) || std::ranges::find(PlayerFiles, inside) != std::end(PlayerFiles);
}

bool IsPlainFileName(std::string_view name) {
	return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\:\"<>|?*") == std::string_view::npos &&
		std::ranges::none_of(name, [](unsigned char c) { return c < ' '; });
}

std::string TruncateChars(std::string_view text, std::size_t maxChars) {
	std::size_t chars = 0, i = 0;
	for (; i < text.size(); ++i) {
		if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) continue; // continuation byte
		if (chars == maxChars) break;
		++chars;
	}
	return i >= text.size() ? std::string(text) : std::string(text.substr(0, i)) + "...";
}

std::string AssetUrl(std::string_view tag, std::string_view asset) {
	return std::string(ReleaseDownloadUrl) + std::string(tag) + "/" + std::string(asset);
}

}
