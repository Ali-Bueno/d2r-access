#include "launcher_logic.h"

#include <cctype>
#include <limits>

namespace D2RAccess::Launcher {

namespace {

constexpr std::string_view LastOnlineKey = "\"User Last Online\"";
constexpr std::string_view LocaleKey     = "text_locale";
constexpr std::string_view LocaleSection = "d2rloader";

std::string_view Trim(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
	return text;
}

}

std::optional<std::uint64_t> ParseLastOnline(std::string_view json) {
	const std::size_t key = json.find(LastOnlineKey);
	if (key == std::string_view::npos) return std::nullopt;
	std::string_view rest = Trim(json.substr(key + LastOnlineKey.size()));
	if (rest.empty() || rest.front() != ':') return std::nullopt;
	rest = Trim(rest.substr(1));
	if (!rest.empty() && rest.front() == '"') rest.remove_prefix(1);
	std::uint64_t value  = 0;
	std::size_t   digits = 0;
	for (const char c : rest) {
		if (c < '0' || c > '9') break;
		const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
		if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return std::nullopt;
		value = value * 10 + digit;
		++digits;
	}
	if (digits == 0) return std::nullopt;
	return value;
}

std::uint64_t AgeDays(std::uint64_t last, std::uint64_t now) {
	return now > last ? (now - last) / NanosPerDay : 0;
}

Verdict Classify(std::optional<std::uint64_t> last, std::uint64_t now) {
	if (!last) return Verdict::Expired;
	const std::uint64_t age = AgeDays(*last, now);
	if (age < static_cast<std::uint64_t>(ValidateAfterDays)) return Verdict::Fresh;
	return age < static_cast<std::uint64_t>(OfflineLimitDays) ? Verdict::Stale : Verdict::Expired;
}

bool DateChanged(std::optional<std::uint64_t> before, std::optional<std::uint64_t> after) {
	return after.has_value() && after != before;
}

std::optional<std::string> ParseTextLocale(std::string_view toml) {
	std::string section;
	while (!toml.empty()) {
		const std::size_t end  = toml.find('\n');
		std::string_view  line = Trim(toml.substr(0, end));
		toml                   = end == std::string_view::npos ? std::string_view{} : toml.substr(end + 1);
		if (line.empty() || line.front() == '#') continue;
		if (line.front() == '[') {
			const std::size_t close = line.find(']');
			section = close == std::string_view::npos ? std::string{} : std::string(Trim(line.substr(1, close - 1)));
			continue;
		}
		if (section != LocaleSection || line.substr(0, LocaleKey.size()) != LocaleKey) continue;
		std::string_view rest = Trim(line.substr(LocaleKey.size()));
		if (rest.empty() || rest.front() != '=') continue;
		rest = Trim(rest.substr(1));
		if (rest.empty() || rest.front() != '"') return std::nullopt;
		rest.remove_prefix(1);
		const std::size_t quote = rest.find('"');
		if (quote == std::string_view::npos || quote == 0) return std::nullopt;
		return std::string(rest.substr(0, quote));
	}
	return std::nullopt;
}

std::wstring ArgumentsAfterProgram(const std::wstring& commandLine) {
	std::size_t i = commandLine.find_first_not_of(L" \t");
	if (i == std::wstring::npos) return {};
	if (commandLine[i] == L'"') {
		const std::size_t close = commandLine.find(L'"', i + 1);
		i                       = close == std::wstring::npos ? commandLine.size() : close + 1;
	} else {
		const std::size_t space = commandLine.find_first_of(L" \t", i);
		i                       = space == std::wstring::npos ? commandLine.size() : space;
	}
	const std::size_t start = commandLine.find_first_not_of(L" \t", i);
	return start == std::wstring::npos ? std::wstring{} : commandLine.substr(start);
}

std::optional<std::wstring> WithoutLeadingFlag(std::wstring_view arguments, std::wstring_view flag) {
	if (!arguments.starts_with(flag)) return std::nullopt;
	const std::wstring_view rest = arguments.substr(flag.size());
	if (!rest.empty() && rest.front() != L' ' && rest.front() != L'\t') return std::nullopt;
	const std::size_t start = rest.find_first_not_of(L" \t");
	return start == std::wstring_view::npos ? std::wstring{} : std::wstring(rest.substr(start));
}

KeyAction NextKeyAction(std::uint64_t now, std::optional<std::uint64_t> lastPress, bool held) {
	if (held) return lastPress && now - *lastPress >= EscapeHoldMs ? KeyAction::Release : KeyAction::None;
	return !lastPress || now - *lastPress >= EscapePeriodMs ? KeyAction::Press : KeyAction::None;
}

}
