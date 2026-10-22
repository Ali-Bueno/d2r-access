#include "tutorial_records.h"
#include "async_log.h"
#include "settings_file.h"
#include "settings_store.h"
#include "tutorial.h"

#include <windows.h>
#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace D2RAccess::TutorialRecords {
namespace {
// A FILETIME counts 100 ns from 1601; Unix seconds count from 1970, 11644473600 s later.
constexpr uint64_t FileTimeTicksPerSecond = 10'000'000, FileTimeUnixEpochSeconds = 11'644'473'600;
// Outside a game, how often a recorded character's save is looked for.
constexpr uint64_t DeletedPollMs = 1000;
// Version 2 gave the Den's rooms and the waypoint steps of their own (Tutorial::FromVersion1);
// a file without this record is version 1. No character name can be empty.
constexpr std::string_view VersionKey = "#version";
constexpr int CurrentVersion = 2;

const D2RL::PluginContext* g_context = nullptr;
std::filesystem::path g_file;
std::map<std::string, int> g_records;
std::set<std::string> g_missing; // records whose save was gone at the last poll
uint64_t g_lastDeletedPoll = 0;

void Write() {
	if (!g_file.empty() && !SettingsStore::Save(g_file, SettingsStore::Encode(g_records)))
		AsyncLog::Warn(g_context, "[tutorial] could not save the tutorial records.");
}

bool IsStep(const std::string& key) { return !key.empty() && key.find('#') == std::string::npos; }

// Every write carries the version, so only a file from before it is brought up to date.
void Upgrade() {
	const std::string version(VersionKey);
	if (g_records.contains(version)) return;
	const bool old = !g_records.empty();
	for (auto& [key, step] : g_records)
		if (IsStep(key)) step = static_cast<int>(Tutorial::FromVersion1(step));
	g_records[version] = CurrentVersion;
	if (!old) return;
	Write();
	AsyncLog::InfoF(g_context, "[tutorial] records brought to version %d", CurrentVersion);
}

// The character's save, <Saved Games>\Diablo II Resurrected\<name>.d2s; empty when the folder is unknown.
std::wstring SavePath(const std::string& name) {
	const std::wstring directory = SettingsFile::SavedGamesDirectory();
	const int length = MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), nullptr, 0);
	if (directory.empty() || length <= 0) return {};
	std::wstring wide(static_cast<size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), wide.data(), length);
	return directory + L"\\" + wide + L".d2s";
}
}

size_t Load(const D2RL::PluginContext* context, std::filesystem::path file) {
	g_context = context;
	g_file = std::move(file);
	g_records.clear();
	if (const auto text = SettingsStore::Load(g_file)) g_records = SettingsStore::Decode(*text);
	Upgrade();
	return static_cast<size_t>(std::count_if(g_records.begin(), g_records.end(), [](const auto& record) { return IsStep(record.first); }));
}

void Clear() {
	g_records.clear();
	g_missing.clear();
	g_file.clear();
	g_context = nullptr;
	g_lastDeletedPoll = 0;
}

// The game keeps a save's creation time as it writes it (MEASURED: yarkey.d2s created 2026-09-14,
// last written 2026-10-20). Made again within 15 s, a save keeps the deleted one's creation time
// (Windows's file name tunnelling), which DropDeleted covers.
std::string Key(const std::string& name) {
	const std::wstring path = SavePath(name);
	WIN32_FILE_ATTRIBUTE_DATA data{};
	if (path.empty() || !GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return name;
	const uint64_t ticks = static_cast<uint64_t>(data.ftCreationTime.dwHighDateTime) << 32 | data.ftCreationTime.dwLowDateTime;
	return name + "@" + std::to_string(ticks / FileTimeTicksPerSecond - FileTimeUnixEpochSeconds);
}

std::optional<int> Step(const std::string& key) {
	const auto record = g_records.find(key);
	if (record == g_records.end()) return std::nullopt;
	return record->second;
}

void SetStep(const std::string& key, int step) {
	if (!IsStep(key)) return;
	g_records[key] = step;
	Write();
}

bool Given(const std::string& key, std::string_view lesson) { return g_records.contains(key + std::string(lesson)); }

void Give(const std::string& key, std::string_view lesson) {
	if (key.empty()) return;
	g_records[key + std::string(lesson)] = 1;
	Write();
}

size_t DropEarlier(const std::string& name, const std::string& key) {
	const auto dropped = std::erase_if(g_records, [&](const auto& record) {
		const auto base = std::string_view(record.first).substr(0, record.first.find('#'));
		return base != key && (base == name || base.starts_with(name + "@"));
	});
	if (dropped) Write();
	return dropped;
}

// Deleted and made again within seconds, the new save carried the old one's creation time and so
// its key (MEASURED 2026-10-21: the new arwen.d2s was created 10:04:23, the deleted one's time), but
// the character select screen leaves the name without a save for longer than a poll. Gone on two
// polls in a row, so a save being rewritten is not taken for a deletion.
void DropDeleted(uint64_t now) {
	if (g_records.empty() || now - g_lastDeletedPoll < DeletedPollMs) return;
	g_lastDeletedPoll = now;
	std::set<std::string> missing;
	for (const auto& [key, step] : g_records) {
		const auto at = key.rfind('@');
		if (at == std::string::npos) continue; // no local save to look for
		const std::wstring path = SavePath(key.substr(0, at));
		if (path.empty()) return;
		if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND) missing.insert(key);
	}
	size_t dropped = 0;
	for (const auto& key : missing)
		if (g_missing.contains(key)) {
			g_records.erase(key);
			++dropped;
			AsyncLog::InfoF(g_context, "[tutorial] %s was deleted: its record is dropped", key.c_str());
		}
	g_missing = std::move(missing);
	for (auto it = g_missing.begin(); it != g_missing.end();) it = g_records.contains(*it) ? std::next(it) : g_missing.erase(it);
	if (dropped) Write();
}
}
