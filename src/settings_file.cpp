#include "settings_file.h"

#include <combaseapi.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <windows.h>

#include <charconv>
#include <unordered_map>
#include <utility>

// SHGetKnownFolderPath ships in Shell32.lib, and the PWSTR it returns is
// freed with CoTaskMemFree from Ole32.lib; pragma-link both here instead of
// touching the CMakeLists target_link_libraries list for two functions.
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace D2RAccess::SettingsFile {

namespace {

// The game's Settings.json is a few KB; this is a generous sanity cap
// against reading a corrupt or unrelated huge file, not a real limit.
constexpr LONGLONG kMaxSettingsFileBytes = 4LL * 1024 * 1024;

// Update commands whose value the game stores under a differently named key
// (observed in Settings.json next to the command that changes it).
struct CommandKeyAlias {
	const char* updateCommand;
	const char* key;
};
constexpr CommandKeyAlias kCommandKeyAliases[] = {
	{ "SetTextLanguage", "Language" },
};

std::wstring ResolveSavedGamesDirectory() {
	PWSTR        knownPath = nullptr;
	std::wstring result;
	// FOLDERID_SavedGames honors a user-redirected Saved Games location (e.g.
	// moved to another drive); reading %USERPROFILE% directly would not.
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SavedGames, 0, nullptr, &knownPath)) && knownPath != nullptr) {
		result = knownPath;
	}
	if (knownPath != nullptr) {
		CoTaskMemFree(knownPath);
	}
	if (!result.empty()) {
		return result;
	}

	// Fallback used only if the shell call itself failed.
	wchar_t    profile[MAX_PATH] {};
	const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) {
		return {};
	}
	return std::wstring(profile) + L"\\Saved Games";
}

constexpr const wchar_t* GameSaveFolder = L"\\Diablo II Resurrected";

const std::wstring& SettingsFilePath() {
	static const std::wstring path = [] {
		const std::wstring directory = SavedGamesDirectory();
		return directory.empty() ? std::wstring() : directory + L"\\Settings.json";
	}();
	return path;
}

// --- Minimal tolerant JSON scanner, sized for exactly this file's shape: a
// flat object of string keys to scalar values. Anything else (nested
// object/array, malformed input) is skipped rather than treated as an error,
// since a partially-readable settings file is still useful. ---

void SkipWhitespace(std::string_view text, size_t& i) {
	while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) {
		++i;
	}
}

// Parses a JSON string starting at text[i] == '"', advancing i past the
// closing quote. Escapes are copied through as their following character
// (correct for \" and \\, the only escapes this file uses; harmless for any
// other backslash escape since values are only ever matched or parsed as
// numbers afterwards, never displayed).
bool ParseJsonString(std::string_view text, size_t& i, std::string& out) {
	if (i >= text.size() || text[i] != '"') {
		return false;
	}
	++i;
	out.clear();
	while (i < text.size() && text[i] != '"') {
		if (text[i] == '\\' && i + 1 < text.size()) {
			out.push_back(text[i + 1]);
			i += 2;
		} else {
			out.push_back(text[i]);
			++i;
		}
	}
	if (i >= text.size()) {
		return false; // unterminated string
	}
	++i; // closing quote
	return true;
}

// Skips one JSON value of any shape at text[i], leaving i just past it.
void SkipJsonValue(std::string_view text, size_t& i) {
	SkipWhitespace(text, i);
	if (i >= text.size()) {
		return;
	}
	if (text[i] == '"') {
		std::string discarded;
		ParseJsonString(text, i, discarded);
		return;
	}
	if (text[i] == '{' || text[i] == '[') {
		const char open  = text[i];
		const char close = (open == '{') ? '}' : ']';
		int        depth = 0;
		while (i < text.size()) {
			if (text[i] == '"') {
				std::string discarded;
				ParseJsonString(text, i, discarded);
				continue;
			}
			if (text[i] == open) {
				++depth;
			} else if (text[i] == close) {
				--depth;
				if (depth == 0) {
					++i;
					return;
				}
			}
			++i;
		}
		return;
	}
	// Bare token (number, true/false/null): read to the next delimiter.
	while (i < text.size() && text[i] != ',' && text[i] != '}' && text[i] != ']' && text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n') {
		++i;
	}
}

bool TryParseNumber(std::string_view token, double& out) {
	if (token.empty()) {
		return false;
	}
	const auto result = std::from_chars(token.data(), token.data() + token.size(), out);
	// Require the whole token to be numeric: rejects "true"/"null" and
	// non-numeric strings (e.g. an IP address) that merely start with digits.
	return result.ec == std::errc() && result.ptr == token.data() + token.size();
}

struct ParsedValues {
	std::unordered_map<std::string, double>      numbers;
	std::unordered_map<std::string, std::string> texts; // string values that are not numbers
};

bool ParseSettingsJson(const std::string& text, ParsedValues& out) {
	out.numbers.clear();
	out.texts.clear();
	size_t i = 0;
	SkipWhitespace(text, i);
	if (i >= text.size() || text[i] != '{') {
		return false; // not an object: nothing to salvage
	}
	++i;
	SkipWhitespace(text, i);

	while (i < text.size() && text[i] != '}') {
		std::string key;
		if (!ParseJsonString(text, i, key)) {
			break; // malformed key: stop, keep whatever was already parsed
		}
		SkipWhitespace(text, i);
		if (i >= text.size() || text[i] != ':') {
			break;
		}
		++i;
		SkipWhitespace(text, i);

		if (i < text.size() && text[i] == '"') {
			std::string value;
			ParseJsonString(text, i, value);
			double number = 0.0;
			if (TryParseNumber(value, number)) {
				out.numbers[NormalizeKey(key)] = number;
			} else {
				out.texts[NormalizeKey(key)] = value;
			}
		} else if (i < text.size() && (text[i] == '{' || text[i] == '[')) {
			SkipJsonValue(text, i); // nested value: not a scalar, ignore
		} else {
			const size_t start = i;
			SkipJsonValue(text, i);
			double number = 0.0;
			if (TryParseNumber(std::string_view(text).substr(start, i - start), number)) {
				out.numbers[NormalizeKey(key)] = number;
			}
		}

		SkipWhitespace(text, i);
		if (i < text.size() && text[i] == ',') {
			++i;
			SkipWhitespace(text, i);
			continue;
		}
		break; // '}' or end of input
	}
	return true;
}

bool ReadFileContents(const std::wstring& path, std::string& outContents, FILETIME& outWriteTime) {
	const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	bool     ok = false;
	FILETIME writeTime {};
	if (GetFileTime(file, nullptr, nullptr, &writeTime)) {
		LARGE_INTEGER size {};
		if (GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart <= kMaxSettingsFileBytes) {
			outContents.resize(static_cast<size_t>(size.QuadPart));
			DWORD bytesRead = 0;
			if (outContents.empty() || (ReadFile(file, outContents.data(), static_cast<DWORD>(outContents.size()), &bytesRead, nullptr) && bytesRead == outContents.size())) {
				outWriteTime = writeTime;
				ok           = true;
			}
		}
	}
	CloseHandle(file);
	return ok;
}

ParsedValues g_values;
FILETIME     g_lastWriteTime {};
bool         g_loaded = false;

}

std::wstring SavedGamesDirectory() {
	static const std::wstring directory = [] {
		const std::wstring base = ResolveSavedGamesDirectory();
		return base.empty() ? std::wstring() : base + GameSaveFolder;
	}();
	return directory;
}

bool Reload() {
	const std::wstring& path = SettingsFilePath();
	if (path.empty()) {
		g_loaded = false;
		return false;
	}

	WIN32_FILE_ATTRIBUTE_DATA attributes {};
	if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes)) {
		// File missing (e.g. the player has never opened Options yet).
		g_loaded = false;
		return false;
	}
	if (g_loaded && CompareFileTime(&attributes.ftLastWriteTime, &g_lastWriteTime) == 0) {
		return true; // unchanged since the last successful read
	}

	std::string contents;
	FILETIME    writeTime {};
	if (!ReadFileContents(path, contents, writeTime)) {
		return g_loaded; // transient read failure (e.g. the game is writing it now): keep old values
	}

	ParsedValues parsed;
	if (!ParseSettingsJson(contents, parsed)) {
		return g_loaded; // malformed file: keep whatever was already loaded
	}

	g_values        = std::move(parsed);
	g_lastWriteTime = writeTime;
	g_loaded        = true;
	return true;
}

std::optional<double> Number(std::string_view normalizedKey) {
	if (!g_loaded) {
		return std::nullopt;
	}
	// Idempotent to call again here even when the caller already normalized
	// the key, and it protects a caller that passed a raw key by mistake.
	const auto it = g_values.numbers.find(NormalizeKey(normalizedKey));
	if (it == g_values.numbers.end()) {
		return std::nullopt;
	}
	return it->second;
}

std::optional<std::string> Text(std::string_view normalizedKey) {
	if (!g_loaded) {
		return std::nullopt;
	}
	const auto it = g_values.texts.find(NormalizeKey(normalizedKey));
	if (it == g_values.texts.end()) {
		return std::nullopt;
	}
	return it->second;
}

std::string NormalizeKey(std::string_view key) {
	std::string result;
	result.reserve(key.size());
	for (const char c : key) {
		if (c >= 'a' && c <= 'z') {
			result.push_back(c);
		} else if (c >= 'A' && c <= 'Z') {
			result.push_back(static_cast<char>(c - 'A' + 'a'));
		} else if (c >= '0' && c <= '9') {
			result.push_back(c);
		}
		// Everything else (spaces, punctuation, parentheses, dots) is dropped.
	}
	return result;
}

std::optional<double> NumberForCommand(std::string_view updateCommand) {
	// SDK update-command naming convention, e.g. "SetNPCSpeech" -> "npcspeech".
	for (const CommandKeyAlias& alias : kCommandKeyAliases) {
		if (updateCommand == alias.updateCommand) {
			return Number(NormalizeKey(alias.key));
		}
	}
	constexpr std::string_view kSetPrefix = "Set";
	const std::string_view     remainder  = updateCommand.starts_with(kSetPrefix) ? updateCommand.substr(kSetPrefix.size()) : updateCommand;
	return Number(NormalizeKey(remainder));
}

}
