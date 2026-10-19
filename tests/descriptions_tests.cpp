#include "descriptions.h"
#include "test_support.h"

using namespace D2RAccess;

int main() {
	Descriptions::Book book;
	book.Parse("# notes are skipped\r\n"
		"Rogue Encampment = Tents and wagons.\r\n"
		"  hangman's tree=A withered tree.  \n"
		"no separator here\n"
		" = no name\n"
		"Cold Plains =\n");
	Check(book.Size() == 2, "notes, lines without a name or words, and lines without = are passed over");
	Check(book.Find("rogue encampment") == "Tents and wagons.", "names match whatever their case");
	Check(book.Find("Cold Plains").empty() && book.Find("Cold Plains", "built in") == "built in",
		"an area without words falls back to the mod's own, else nothing");
	Check(book.Find("Hangman's Tree", "built in") == "A withered tree.", "the player's words win over the mod's");

	Check(book.FirstTime("Hangman's Tree", "built in") == "A withered tree.", "a landmark's words the first time");
	Check(book.FirstTime("HANGMAN'S TREE", "built in").empty(), "only its name after that");
	Check(book.FirstTime("Stairs", "").empty(), "a landmark without words is said by its name");

	Check(book.HintDue("Rogue Encampment"), "the hint the first time a described area is entered");
	Check(!book.HintDue("Rogue Encampment"), "not again");
	Check(!book.HintDue("Blood Moor"), "no hint where nothing is written");

	// The file saved while the game runs (a tester, 2026-10-19): changed words are news again.
	Descriptions::Book live;
	live.Parse("Hangman's Tree = A withered tree.\nCold Plains = Grass.\n");
	(void)live.FirstTime("Hangman's Tree", "");
	(void)live.HintDue("Cold Plains");
	live.Replace("Hangman's Tree = A withered tree.\nCold Plains = Tall grass.\nDen of Evil = A cave.\n");
	Check(live.FirstTime("Hangman's Tree", "").empty(), "words that did not change are not said again");
	Check(live.HintDue("Cold Plains") && live.Find("Den of Evil") == "A cave.", "changed and new words are");
	live.Replace("Den of Evil = A cave.\n");
	Check(live.Find("Cold Plains").empty() && live.Size() == 1, "a line taken out is gone");

	// An area as its banner names it (a tester, 2026-10-19: "The Cave Level 1" said nothing).
	Descriptions::Book areas;
	areas.Parse("The Cave Level 1 = A damp cave.\nSwampy Pit Level 1 = A swamp.\nEntering The Hole Level 1 = A hole.\n");
	Check(areas.FindArea("Cave Level 1", "Entering The Cave Level 1") == "A damp cave.", "the banner's article is accepted");
	Check(areas.FindArea("Hole Level 1", "Entering The Hole Level 1") == "A hole.", "and the banner's whole text");
	Check(areas.FindArea("Pit Level 1", "Entering The Pit Level 1").empty(), "another area ending in the same words is not it");
	Check(areas.FindArea("Cave Level 1", "Entering The Blood Moor").empty(), "nor is a key the current banner does not say");
	areas.Parse("Cave Level 1 = The level's own name wins.\n");
	Check(areas.FindArea("Cave Level 1", "Entering The Cave Level 1") == "The level's own name wins.", "the level's own name first");
	Passed();
	return 0;
}
