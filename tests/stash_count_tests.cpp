#include "stash_count.h"
#include "test_support.h"

using D2RAccess::StashCount::WithCount;

int main() {
	Check(WithCount("Chipped Topaz", "5", true, ", ") == "Chipped Topaz, 5", "the count follows the name");
	Check(WithCount("El Rune", "99", true, ", ") == "El Rune, 99", "up to the stack limit");
	Check(WithCount("Chipped Topaz", "0", false, ", ") == "Chipped Topaz", "an empty slot hides its count, so the name stands alone");
	Check(WithCount("Chipped Topaz", "", true, ", ") == "Chipped Topaz", "an unreadable count is left out");
	Check(WithCount("Chipped Topaz", "x5", true, ", ") == "Chipped Topaz", "text that is not a number is left out");
	Passed();
}
