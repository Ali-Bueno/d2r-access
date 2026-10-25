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

	Check(book.HintDue(1, "Rogue Encampment", "Entering The Rogue Encampment"), "the hint the first time a described area is entered");
	Check(!book.HintDue(1, "Rogue Encampment", "Entering The Rogue Encampment"), "not again");
	Check(!book.HintDue(2, "Blood Moor", "Entering The Blood Moor"), "no hint where nothing is written");

	// The file saved while the game runs (a tester, 2026-10-19): changed words are news again.
	Descriptions::Book live;
	live.Parse("Hangman's Tree = A withered tree.\nCold Plains = Grass.\n");
	(void)live.FirstTime("Hangman's Tree", "");
	(void)live.HintDue(3, "Cold Plains", "Entering The Cold Plains");
	live.Replace("Hangman's Tree = A withered tree.\nCold Plains = Tall grass.\nDen of Evil = A cave.\n");
	Check(live.FirstTime("Hangman's Tree", "").empty(), "words that did not change are not said again");
	Check(live.HintDue(3, "Cold Plains", "Entering The Cold Plains") && live.Find("Den of Evil") == "A cave.", "changed and new words are");
	live.Replace("Den of Evil = A cave.\n");
	Check(live.Find("Cold Plains").empty() && live.Size() == 1, "a line taken out is gone");

	// An area as its banner names it (a tester, 2026-10-19: "The Cave Level 1" said nothing).
	Descriptions::Book areas;
	areas.Parse("The Cave Level 1 = A damp cave.\nSwampy Pit Level 1 = A swamp.\nEntering The Hole Level 1 = A hole.\n");
	Check(areas.FindArea(9, "Cave Level 1", "Entering The Cave Level 1") == "A damp cave.", "the banner's article is accepted");
	Check(areas.FindArea(11, "Hole Level 1", "Entering The Hole Level 1") == "A hole.", "and the banner's whole text");
	Check(areas.FindArea(12, "Pit Level 1", "Entering The Pit Level 1").empty(), "another area ending in the same words is not it");
	Check(areas.FindArea(9, "Cave Level 1", "Entering The Blood Moor").empty(), "nor is a key the current banner does not say");
	areas.Parse("Cave Level 1 = The level's own name wins.\n");
	Check(areas.FindArea(9, "Cave Level 1", "Entering The Cave Level 1") == "The level's own name wins.", "the level's own name first");

	// The mod's book is read first, the player's after it (a tester's words shipped, 2026-10-23).
	Descriptions::Book shipped;
	shipped.Parse("Blood Moor = The mod's moor.\nCold Plains = The mod's plains.");
	shipped.Parse("Blood Moor = My moor.\n");
	Check(shipped.Find("Blood Moor") == "My moor." && shipped.Find("Cold Plains") == "The mod's plains.",
		"the player's line replaces the mod's, the mod's others stay");
	shipped.Replace("Blood Moor = The mod's moor.\nCold Plains = The mod's plains.\n"); // the player's file emptied
	Check(shipped.Find("Blood Moor") == "The mod's moor.", "the player's line taken out leaves the mod's");

	// A level of its own by its Levels.txt Id (a tester, 2026-10-24: Act II's and Act III's "Sewers Level 1").
	Descriptions::Book sewers;
	sewers.Parse("Sewers Level 1 [47] = Lut Gholein's.\nKurast sewers [92] = Kurast's.\nSewers Level 1 = Any other.\nTown [1x] = Not an Id.\n");
	Check(sewers.FindArea(47, "Sewers Level 1", "Entering The Sewers Level 1") == "Lut Gholein's."
		&& sewers.FindArea(92, "Sewers Level 1", "Entering The Sewers Level 1") == "Kurast's.", "each level its own words, whatever name is written");
	Check(sewers.FindArea(93, "Sewers Level 2", "Entering The Sewers Level 2").empty()
		&& sewers.FindArea(5, "Sewers Level 1", "") == "Any other.", "a level without its own line falls back to the name");
	Check(sewers.Find("Town [1x]") == "Not an Id.", "brackets without a number are part of the name");
	sewers.Parse("Sewers [47] = Mine.\n");
	Check(sewers.FindArea(47, "Sewers Level 1", "") == "Mine.", "a later line for the same Id replaces the first");
	Check(sewers.HintDue(92, "Sewers Level 1", "") && sewers.HintDue(47, "Sewers Level 1", "")
		&& !sewers.HintDue(47, "Sewers Level 1", ""), "each level gets its hint once");
	Passed();
	return 0;
}
