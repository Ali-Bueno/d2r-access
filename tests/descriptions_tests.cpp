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
	Passed();
	return 0;
}
