#pragma once

#include <D2RLPlugin/api.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Hand-written words for an area or a landmark, by the name the mod says for it:
// the player's own descriptions.txt in the plugin folder, then the mod's built-in
// ones. docs/descriptions.md
namespace D2RAccess::Descriptions {

// "Name = words" lines; # starts a note. Keys are folded (Fold), so the case of a
// name does not matter. A name ending in a level's Levels.txt Id in brackets,
// "Sewers Level 1 [47]", is that level's alone (LevelKey).
class Book {
public:
	void Parse(std::string_view text);
	// The book read again: a name whose words changed is new again for FirstTime and HintDue.
	void Replace(std::string_view text);
	// The words for `name`: the book's own, else `builtIn`.
	std::string Find(std::string_view name, std::string_view builtIn = {}) const;
	// An area's words by its level's Id, else by its level name, else by a name its banner ends with
	// that ends with the level name: "The Cave Level 1" from "Entering The Cave Level 1" (a tester,
	// 2026-10-19), never "Swampy Pit Level 1" for the Pit.
	std::string FindArea(int level, std::string_view name, std::string_view banner) const;
	// The words to say instead of `name` the first time it is said; empty after that.
	std::string FirstTime(std::string_view name, std::string_view builtIn);
	// True the first time an area that has words (FindArea) is entered.
	bool HintDue(int level, std::string_view name, std::string_view banner);
	size_t Size() const { return entries_.size(); }
private:
	std::string AreaKey(int level, std::string_view name, std::string_view banner) const;
	std::unordered_map<std::string, std::string> entries_;
	std::unordered_set<std::string> told_, hinted_;
};
std::string Fold(std::string_view name);
// The key a level's own line is kept under: "[47]".
std::string LevelKey(int level);

// The service: one book for the run, read from `file` (a commented template is
// written when it is missing) and read again within a second of being saved, so
// words can be tried while the game runs (a tester, 2026-10-19).
void Load(const D2RL::PluginContext* context, const std::filesystem::path& file);
// Writes every name a description can be given, beside the book, in the game's language.
void WriteNames(const std::vector<std::string>& areas, const std::vector<std::string>& landmarks);
std::string Find(std::string_view name, std::string_view builtIn = {});
// Book::FindArea with the last area banner the game showed (NoteBanner).
std::string FindArea(int level, std::string_view name);
void NoteBanner(std::string_view banner);
// The level the player is in and its name, for the hint that follows its banner.
void NoteLevel(int level, std::string name);
std::string FirstTime(std::string_view name, std::string_view builtIn);
// Book::HintDue for the noted level and the banner just shown.
bool HintDue(std::string_view banner);
// The switch: landmark words on first reach. Asked-for words are said either way.
bool Enabled();
void SetEnabled(bool on);
// The area hint after a described area's banner; off by default, so the lore waits to be asked for
// (a tester, 2026-10-26).
bool HintEnabled();
void SetHintEnabled(bool on);
}
