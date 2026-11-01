#pragma once

#include "sealed_text.h"

#include <D2RLPlugin/api.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

// Hand-written words for an area, by its level's Id, or a landmark, by its English name: in the
// game's language with English under it, sealed in the plugin. docs/descriptions.md
namespace D2RAccess::Descriptions {

// "Name [Id] = words" lines for areas, "Name = words" for landmarks; # starts a note. Keys are
// folded (Fold), so the case of a name does not matter.
class Book {
public:
	// Returns how many lines were neither notes nor "name = words".
	size_t Parse(std::string_view text);
	// New words (another language), keeping what was already said and hinted.
	void Replace(std::string_view text);
	// The words for `name`: the book's own, else `builtIn`.
	std::string Find(std::string_view name, std::string_view builtIn = {}) const;
	// An area's words by its level's Levels.txt Id.
	std::string FindArea(int level) const;
	// The words to say instead of `name` the first time it is said; empty after that.
	std::string FirstTime(std::string_view name, std::string_view builtIn);
	// True the first time an area that has words is entered.
	bool HintDue(int level);
	const std::unordered_map<std::string, std::string>& Entries() const { return entries_; }
	size_t Size() const { return entries_.size(); }
	// A line's key: "[47]" for "Sewers Level 1 [47]", else the folded name.
	static std::string Keyed(std::string_view name);
private:
	std::unordered_map<std::string, std::string> entries_;
	std::unordered_set<std::string> told_, hinted_;
};
std::string Fold(std::string_view name);
// The key a level's line is kept under: "[47]".
std::string LevelKey(int level);
// The text a language's book is read from: English first, so a line the language lacks is English.
std::string Layered(const SealedText::Texts& texts, std::string_view locale);

// The words sealed in this plugin by locale ("enUS"); empty, and logged, when missing or altered.
SealedText::Texts OpenBundled(const D2RL::PluginContext* context);
// The service: `texts` in the language `locale()` names, read again when it changes.
void Load(const D2RL::PluginContext* context, SealedText::Texts texts, std::string_view (*locale)());
std::string FindArea(int level);
// The level the player is in, for the hint that follows its banner.
void NoteLevel(int level);
std::string FirstTime(std::string_view name, std::string_view builtIn);
// Book::HintDue for the noted level.
bool HintDue();
// The switch: landmark words on first reach. Asked-for words are said either way.
bool Enabled();
void SetEnabled(bool on);
// The area hint after a described area's banner; off by default, so the lore waits to be asked for
// (a tester, 2026-10-26).
bool HintEnabled();
void SetHintEnabled(bool on);
}
