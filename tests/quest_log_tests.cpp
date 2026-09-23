#include "quest_log.h"
#include "test_support.h"

#include <map>
#include <string>

using namespace D2RAccess;

namespace {

// Stands in for the game's string table. Only the keys a case needs are
// present, so an unexpected lookup shows up as an empty string, exactly as the
// real service behaves for an unknown key.
std::map<std::string, std::string, std::less<>> g_strings;

std::string Resolve(std::string_view key) {
	const auto found = g_strings.find(key);
	return found == g_strings.end() ? std::string() : found->second;
}

const Generated::Quest& Quest(std::string_view widget) {
	const auto* quest = QuestLog::ForWidget(widget);
	Check(quest != nullptr, "the catalog knows the quest buttons the game ships");
	return *quest;
}

}

int main() {
	// The button names are not in quest order. Classic MENU icons identify
	// quests; HD QuestIcons identify visual positions and differ on these rows.
	const auto& den = Quest("Quest0");
	Check(den.act == 1 && den.number == 1, "Quest0 is act 1 quest 1");
	const auto& tools = Quest("Quest2");
	Check(tools.act == 1 && tools.number == 3, "Quest2 is Tools of the Trade");
	const auto& cain = Quest("Quest3");
	Check(cain.act == 1 && cain.number == 4 && std::string(cain.titleKey) == "qstsa1q4",
		"Quest3 is The Search for Cain, as the live detail pane shows");
	const auto& tower = Quest("Quest4");
	Check(tower.act == 1 && tower.number == 5, "Quest4 is The Forgotten Tower");
	Check(Quest("Quest15").act == 3 && Quest("Quest15").number == 4,
		"Quest15 is The Golden Bird in the classic layout");
	Check(Quest("Quest12").act == 3 && Quest("Quest12").number == 1,
		"Quest12 is Lam Esen's Tome in the classic layout");
	Check(QuestLog::ForWidget("Quest27") == nullptr, "unknown widgets are not quests");
	Check(QuestLog::ForWidget("") == nullptr, "an unnamed widget is not a quest");
	Check(QuestLog::ForWidget("Cancel") == nullptr, "other panel controls are not quests");

	// Every act's quests are present exactly once, and act 4 has only three.
	int perAct[6] = {};
	for (const auto& quest : Generated::Quests) {
		Check(quest.act >= 1 && quest.act <= 5, "every quest belongs to a real act");
		++perAct[quest.act];
	}
	Check(perAct[1] == 6 && perAct[2] == 6 && perAct[3] == 6 && perAct[4] == 3 && perAct[5] == 6,
		"the catalog covers all 27 quests with act 4's short list");

	Check(std::string(den.titleKey) == "qstsa1q1", "the title key follows the game's own naming");
	Check(QuestLog::Title(den, Resolve).empty(), "an unresolved quest name stays empty rather than guessing");
	g_strings["qstsa1q1"] = "El Cubil del Mal";
	Check(QuestLog::Title(den, Resolve) == "El Cubil del Mal", "the quest name comes from the game's tables");
	Check(QuestLog::Title(den, nullptr).empty(), "a missing resolver is survivable");

	// Act 5 quest 1 is the one quest whose steps Blizzard keyed without the
	// second 's'; the catalog must still carry them.
	const auto& siege = Quest("Quest21");
	Check(std::string(siege.titleKey) == "qstsa5q1", "act 5 quest 1 keeps the regular title key");
	Check(siege.stepCount > 0, "act 5 quest 1 still has its objectives despite the key typo");
	bool irregular = false;
	for (uint8_t i = 0; i < siege.stepCount; ++i) {
		if (std::string(siege.stepKeys[i]).rfind("qsta5q1", 0) == 0) irregular = true;
	}
	Check(irregular, "the shipped 'qsta5q1' step keys are the ones recorded");

	// Objectives read back in the order the game ships them, and a counter line
	// is held back until there is a number to put in it.
	g_strings["qstsa1q11"] = "Busca el cubil.";
	g_strings["qstsa1q12"] = "Mata a todos los monstruos del cubil.";
	g_strings["qstsa1q14"] = "Monstruos restantes: %d";
	g_strings["qstsa1q140"] = "Falta un monstruo.";
	g_strings["qstsa1q15"] = "Vuelve con Akara.";
	const auto objectives = QuestLog::Objectives(den, Resolve);
	Check(objectives.size() == 4, "blank lines and unfilled counters are left out");
	Check(objectives[0] == "Busca el cubil." && objectives[3] == "Vuelve con Akara.",
		"objectives keep the game's own order");
	for (const auto& line : objectives) Check(!QuestLog::NeedsValue(line), "no objective is spoken with a placeholder");

	Check(QuestLog::NeedsValue("Monstruos restantes: %d"), "a counter line is recognised");
	Check(!QuestLog::NeedsValue("100%% completado"), "an escaped percent sign is not a value");
	Check(!QuestLog::NeedsValue("Mata a todos los monstruos."), "plain text needs no value");

	Check(QuestLog::Counter(den, 3, Resolve) == "Monstruos restantes: 3", "the counter is filled from the game's sentence");
	Check(QuestLog::Counter(den, 12, Resolve) == "Monstruos restantes: 12", "a two-digit count does not truncate the sentence");
	Check(QuestLog::Counter(den, 1, Resolve) == "Falta un monstruo.", "the game's own singular wording wins for the last one");
	g_strings.erase("qstsa1q140");
	Check(QuestLog::Counter(den, 1, Resolve) == "Monstruos restantes: 1", "without a singular line the counter still reads");
	Check(QuestLog::Counter(tower, 3, Resolve).empty(), "a quest with no counter reports none");

	Passed();
}
