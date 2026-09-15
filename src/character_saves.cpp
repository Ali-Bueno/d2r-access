#include "character_saves.h"

#include "settings_file.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

namespace D2RAccess::CharacterSaves {

namespace {

constexpr uint32_t SaveMagic          = 0xAA55AA55; // bytes 55 AA 55 AA, read little-endian
constexpr uint32_t SupportedVersion   = 105;
constexpr size_t   VersionOffset      = 0x04;
constexpr size_t   ClassOffset        = 0x18;
constexpr size_t   LevelOffset        = 0x1B;
constexpr size_t   LastPlayedOffset   = 0x20;
constexpr size_t   NameOffset         = 0x12B;
constexpr size_t   NameLength         = 16;
constexpr size_t   HeaderBytes        = NameOffset + NameLength;
constexpr const wchar_t* SaveExtension = L"*.d2s";

// String-table keys, in the save file's class numbering.
constexpr const char* ClassKeys[] = { "Amazon", "Sorceress", "Necromancer", "Paladin", "Barbarian", "Druid", "Assassin" };

uint32_t ReadU32(const unsigned char* bytes, size_t offset) {
	uint32_t value = 0;
	std::memcpy(&value, bytes + offset, sizeof(value));
	return value;
}

// Fills `out` or returns why the file was skipped.
const char* ReadHeader(const std::wstring& path, Character& out, DWORD& lastError) {
	const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		lastError = GetLastError();
		return "cannot open";
	}
	unsigned char header[HeaderBytes] {};
	DWORD         read = 0;
	const bool    ok   = ReadFile(file, header, sizeof(header), &read, nullptr) && read == sizeof(header);
	lastError          = ok ? 0 : GetLastError();
	CloseHandle(file);
	if (!ok) {
		return "short read";
	}
	if (ReadU32(header, 0) != SaveMagic) {
		return "bad magic";
	}
	if (ReadU32(header, VersionOffset) != SupportedVersion) {
		return "unsupported version";
	}
	const char* name = reinterpret_cast<const char*>(header + NameOffset);
	out.name.assign(name, strnlen(name, NameLength));
	out.classIndex = header[ClassOffset];
	out.level      = header[LevelOffset];
	out.lastPlayed = ReadU32(header, LastPlayedOffset);
	return out.name.empty() ? "empty name" : nullptr;
}

std::string Narrow(const std::wstring& text) {
	std::string result;
	for (const wchar_t c : text) {
		result.push_back(c < 0x80 ? static_cast<char>(c) : '?');
	}
	return result;
}

}

std::vector<Character> Load(const D2RL::PluginContext* context) {
	std::vector<Character> characters;
	const std::wstring     directory = SettingsFile::SavedGamesDirectory();
	if (directory.empty()) {
		D2RL::LogWarnF(context, "[saves] saved games folder not resolved");
		return characters;
	}
	WIN32_FIND_DATAW found {};
	const HANDLE     search = FindFirstFileW((directory + L"\\" + SaveExtension).c_str(), &found);
	if (search == INVALID_HANDLE_VALUE) {
		D2RL::LogWarnF(context, "[saves] no %ls in \"%s\" (error %lu)", SaveExtension, Narrow(directory).c_str(), GetLastError());
		return characters;
	}
	do {
		Character character;
		DWORD     lastError = 0;
		if (const char* skipped = ReadHeader(directory + L"\\" + found.cFileName, character, lastError); skipped != nullptr) {
			D2RL::LogWarnF(context, "[saves] %s skipped: %s (error %lu)", Narrow(found.cFileName).c_str(), skipped, lastError);
		} else {
			D2RL::LogInfoF(context, "[saves] %s: \"%s\" class %d level %d last played %lu", Narrow(found.cFileName).c_str(), character.name.c_str(), character.classIndex, character.level, static_cast<unsigned long>(character.lastPlayed));
			characters.push_back(std::move(character));
		}
	} while (FindNextFileW(search, &found));
	FindClose(search);
	std::stable_sort(characters.begin(), characters.end(), [](const Character& a, const Character& b) { return a.lastPlayed > b.lastPlayed; });
	return characters;
}

const char* ClassKey(int classIndex) {
	if (classIndex < 0 || classIndex >= static_cast<int>(std::size(ClassKeys))) {
		return nullptr;
	}
	return ClassKeys[classIndex];
}

}
