#include "launcher_logic.h"

#include <cctype>
#include <cwctype>
#include <limits>

namespace D2RAccess::Launcher {

namespace {

constexpr std::string_view LastOnlineKey = "\"User Last Online\"";
constexpr std::string_view FirstTimeKey  = "\"First Time\"";
constexpr std::string_view Utf8Bom       = "\xEF\xBB\xBF";
constexpr std::wstring_view SteamCommonFolder = L"\\steamapps\\common";
constexpr std::string_view BattleNetAgentFolder = "battle.net/agent"; // every product.db names its own agent
constexpr std::string_view LocaleKey     = "text_locale";
constexpr std::string_view LocaleSection = "d2rloader";

std::string_view Trim(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
	return text;
}

// The quoted value after the first quoted `key` of a Valve KeyValues text.
std::optional<std::string_view> VdfValue(std::string_view vdf, std::string_view key) {
	const std::string quoted = "\"" + std::string(key) + "\"";
	const std::size_t at     = vdf.find(quoted);
	if (at == std::string_view::npos) return std::nullopt;
	std::string_view rest = Trim(vdf.substr(at + quoted.size()));
	if (!rest.starts_with('"')) return std::nullopt;
	rest.remove_prefix(1);
	const std::size_t end = rest.find('"');
	if (end == std::string_view::npos) return std::nullopt;
	return rest.substr(0, end);
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

std::optional<std::string> WithFirstTimeMarked(std::string_view json) {
	const std::size_t key = json.find(FirstTimeKey);
	if (key != std::string_view::npos) {
		std::size_t at = key + FirstTimeKey.size();
		while (at < json.size() && std::isspace(static_cast<unsigned char>(json[at]))) ++at;
		if (at == json.size() || json[at] != ':') return std::nullopt;
		++at;
		while (at < json.size() && std::isspace(static_cast<unsigned char>(json[at]))) ++at;
		std::size_t end = at;
		while (end < json.size() && json[end] >= '0' && json[end] <= '9') ++end;
		if (end == at || json.substr(at, end - at) == "1") return std::nullopt;
		std::string marked(json);
		marked.replace(at, end - at, "1");
		return marked;
	}
	std::string_view head = json;
	if (head.starts_with(Utf8Bom)) head.remove_prefix(Utf8Bom.size());
	head = Trim(head);
	if (!head.starts_with('{')) return std::nullopt;
	const std::size_t open  = json.find('{');
	const bool        empty = Trim(json.substr(open + 1)).starts_with('}');
	std::string marked(json);
	marked.insert(open + 1, empty ? "\n    \"First Time\": 1" : "\n    \"First Time\": 1,");
	return marked;
}

bool InSteamLibrary(std::wstring_view gameFolder) {
	std::wstring folder;
	for (const wchar_t c : gameFolder) folder += c == L'/' ? L'\\' : static_cast<wchar_t>(std::towlower(c));
	while (!folder.empty() && folder.back() == L'\\') folder.pop_back();
	const std::size_t game = folder.find_last_of(L'\\');
	return game != std::wstring::npos && game + 1 < folder.size() && folder.substr(0, game).ends_with(SteamCommonFolder);
}

BattleNetListing ListingIn(std::string_view productDb, std::string_view gameFolder) {
	const auto pathKey = [](std::string_view path) {
		std::string key;
		key.reserve(path.size());
		for (const char c : path) key += c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return key;
	};
	const std::string db = pathKey(productDb);
	std::string folder   = pathKey(gameFolder);
	while (!folder.empty() && folder.back() == '/') folder.pop_back();
	if (folder.empty() || db.find(BattleNetAgentFolder) == std::string::npos) return BattleNetListing::Unknown;
	return db.find(folder) != std::string::npos ? BattleNetListing::Listed : BattleNetListing::NotListed;
}

std::optional<std::string> ManifestAppIdFor(std::string_view manifest, std::string_view installDir) {
	const auto folder = VdfValue(manifest, "installdir");
	const auto appId  = VdfValue(manifest, "appid");
	if (!folder || !appId || appId->empty() || folder->size() != installDir.size()) return std::nullopt;
	for (std::size_t i = 0; i < installDir.size(); ++i) {
		if (std::tolower(static_cast<unsigned char>((*folder)[i])) != std::tolower(static_cast<unsigned char>(installDir[i]))) return std::nullopt;
	}
	for (const char c : *appId) if (c < '0' || c > '9') return std::nullopt;
	return std::string(*appId);
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
