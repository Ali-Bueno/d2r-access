#include "quest_seals.h"
#include "test_support.h"

using namespace D2RAccess;

namespace {
void CountsFromModes() {
	QuestSeals::Memory memory;
	Check(QuestSeals::Count == 5, "the game has five Chaos Sanctuary seals");
	Check(memory.Left(1) == QuestSeals::Count && !memory.TakeOpening(1), "unseen seals are all still to open");
	memory.Observe(1, 1, false);
	memory.Observe(1, 2, false);
	Check(memory.Left(1) == 5 && !memory.TakeOpening(1), "a shut seal is not an opening");
	memory.Observe(1, 2, true);
	Check(memory.Left(1) == 4, "a seal seen open is counted off");
	const auto opening = memory.TakeOpening(1);
	Check(opening && *opening == 4 && !memory.TakeOpening(1), "an opening is told once, with the seals left");
	memory.Observe(1, 2, true);
	memory.Observe(1, 2, false);
	Check(memory.Left(1) == 4 && !memory.TakeOpening(1), "an open seal stays open and is not told again");
	memory.Observe(1, 3, true);
	Check(memory.Left(1) == 3 && !memory.TakeOpening(1), "one found already open counts, but is no opening the player saw");
	memory.Observe(1, 0, true);
	memory.Observe(1, QuestSeals::Count + 1, true);
	Check(memory.Left(1) == 3, "a number that is no seal changes nothing");
	for (uint8_t seal = 1; seal <= QuestSeals::Count; ++seal) memory.Observe(1, seal, false);
	for (uint8_t seal = 1; seal <= QuestSeals::Count; ++seal) memory.Observe(1, seal, true);
	const auto last = memory.TakeOpening(1);
	Check(last && *last == 0 && memory.Left(1) == 0, "the last opening says none are left");
	Check(memory.Left(2) == QuestSeals::Count, "another game has its own seals");
	memory.Observe(2, 1, false);
	Check(memory.Left(1) == QuestSeals::Count && !memory.TakeOpening(2), "a new game forgets the last one's seals");
}

void SpokenCount() {
	const auto& english = Strings::For(Strings::Language::enUS);
	Check(QuestSeals::LeftText(3, english) == "Seals left: 3", "the seals left are counted aloud");
	Check(QuestSeals::LeftText(1, english) == "Seals left: 1", "one left reads the same way");
	Check(QuestSeals::LeftText(0, english) == english.sealsAllOpen, "none left says that all are open");
}
}

int main() {
	CountsFromModes();
	SpokenCount();
	Passed();
	return 0;
}
