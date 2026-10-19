#include "test_support.h"
#include "text_field_edit.h"

int main() {
	using namespace D2RAccess::TextField;
	Check(Classify("ab", "ab").kind == EditKind::None, "no change says nothing");
	const Edit added = Classify("ab", "abc");
	Check(added.kind == EditKind::Added && added.text == "c", "an appended key is echoed");
	const Edit pasted = Classify("", "xyz");
	Check(pasted.kind == EditKind::Added && pasted.text == "xyz", "several appended characters are echoed together");
	Check(Classify("ab", "ab ").text == " ", "an appended space is reported as a single space");
	const Edit removed = Classify("abc", "ab");
	Check(removed.kind == EditKind::Removed && removed.text == "c", "a deletion names what was removed");
	const Edit replaced = Classify("abc", "xy");
	Check(replaced.kind == EditKind::Replaced && replaced.text == "xy", "a wholesale change reads the new contents");
	Passed();
}
