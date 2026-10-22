#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// Which step each character's tutorial reached and which lessons it was given, kept in
// <pluginDirectory>/d2r-access/tutorial.txt as `name@created=step` and `name@created#lesson=1`
// (docs/tutorial.md). UI thread only.
namespace D2RAccess::TutorialRecords {
// Reads the file, bringing records of an older version up to date; the count read.
size_t Load(const D2RL::PluginContext* context, std::filesystem::path file);
void Clear();

// A character's key: its name and when its save was created, so one deleted and made again
// under the same name is another character; the name alone without a local save.
std::string Key(const std::string& name);
std::optional<int> Step(const std::string& key);
void SetStep(const std::string& key, int step);
bool Given(const std::string& key, std::string_view lesson);
void Give(const std::string& key, std::string_view lesson);
// Records of an earlier character of this name, or from before keys had a time; the count dropped.
size_t DropEarlier(const std::string& name, const std::string& key);
// Outside a game: a record whose save is gone on two polls in a row is dropped.
void DropDeleted(uint64_t now);
}
