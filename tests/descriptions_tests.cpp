#include "descriptions.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace D2RAccess;

namespace {
// The last Id in Levels.txt (Colossal Summit); every level from 1 has English words.
constexpr int LastLevel = 137;

std::string ReadFile(const std::filesystem::path& path) {
	std::ifstream input(path, std::ios::binary);
	std::stringstream text;
	text << input.rdbuf();
	return text.str();
}

void CheckBook() {
	Descriptions::Book book;
	const size_t rejected = book.Parse("# notes are skipped\r\n"
		"Rogue Encampment [1] = Tents and wagons.\r\n"
		"  hangman's tree=A withered tree.  \n"
		"no separator here\n"
		" = no name\n"
		"Cold Plains [3] =\n"
		"Blood Moor = A name without an Id.\n");
	Check(rejected == 3, "lines without a name, words or = are counted as rejected");
	Check(book.Size() == 3, "the rest are kept");
	Check(book.FindArea(1) == "Tents and wagons.", "an area by its level's Id");
	Check(book.FindArea(2).empty() && book.FindArea(3).empty() && book.FindArea(0).empty(),
		"an area answers by its Id alone");
	Check(book.Find("Hangman's Tree", "built in") == "A withered tree.", "a landmark by its name, any case");
	Check(book.Find("Stairs", "built in") == "built in", "the mod's own words where the book has none");

	Check(book.FirstTime("Hangman's Tree", "built in") == "A withered tree.", "a landmark's words the first time");
	Check(book.FirstTime("HANGMAN'S TREE", "built in").empty(), "only its name after that");
	Check(book.FirstTime("Stairs", "").empty(), "a landmark without words is said by its name");

	Check(book.HintDue(1) && !book.HintDue(1), "the hint the first time a described area is entered, not again");
	Check(!book.HintDue(3), "no hint where nothing is written");

	book.Replace("Rogue Encampment [1] = Zelte.\nHangman's Tree = Ein Baum.\nCold Plains [3] = Gras.\n");
	Check(book.FindArea(1) == "Zelte." && book.FindArea(3) == "Gras.", "another language's words");
	Check(book.FirstTime("Hangman's Tree", "").empty() && !book.HintDue(1),
		"what was said or hinted in one language is not again in another");
	Check(book.HintDue(3), "a newly described area still gets its hint");

	Descriptions::Book sewers;
	sewers.Parse("Sewers Level 1 [47] = Lut Gholein's.\nSewers Level 1 [92] = Kurast's.\nAny words [47] = Mine.\n");
	Check(sewers.FindArea(92) == "Kurast's.", "two levels sharing a name keep their own words");
	Check(sewers.FindArea(47) == "Mine.", "a later line for the same Id replaces the first");
}

void CheckLayers() {
	const SealedText::Texts texts{{"enUS", "Blood Moor [2] = A moor.\nCold Plains [3] = Plains.\n"},
		{"deDE", "Blutmoor [2] = Ein Moor.\n"}};
	Descriptions::Book german;
	german.Parse(Descriptions::Layered(texts, "deDE"));
	Check(german.FindArea(2) == "Ein Moor." && german.FindArea(3) == "Plains.",
		"a language's own words, English where it has none");
	Descriptions::Book french;
	french.Parse(Descriptions::Layered(texts, "frFR"));
	Check(french.FindArea(2) == "A moor.", "a language without a file is English");
	Check(Descriptions::Layered(texts, "enUS") == texts.at("enUS"), "English is read once");
	Check(Descriptions::Layered({}, "deDE").empty(), "nothing when the words could not be opened");
}

// The files the plugin is sealed from: every line well formed, every level in English, and no
// language with a line English lacks (a mistyped Id).
void CheckData() {
	SealedText::Texts texts;
	for (const auto& entry : std::filesystem::directory_iterator(D2R_DESCRIPTIONS_DIR))
		if (entry.path().extension() == ".txt") texts[entry.path().stem().string()] = ReadFile(entry.path());
	Check(texts.contains("enUS"), "the English file is there");
	Descriptions::Book english;
	Check(english.Parse(texts.at("enUS")) == 0, "every English line is a note or name = words");
	for (int level = 1; level <= LastLevel; ++level)
		if (english.FindArea(level).empty()) Check(false, "level " + std::to_string(level) + " has English words");
	for (const auto& [locale, text] : texts) {
		Descriptions::Book book;
		Check(book.Parse(text) == 0, locale + ": every line is a note or name = words");
		for (const auto& [key, words] : book.Entries())
			Check(english.Entries().contains(key), locale + ": \"" + key + "\" is also in English");
	}
}
}

int main() {
	CheckBook();
	CheckLayers();
	CheckData();
	Passed();
	return 0;
}
