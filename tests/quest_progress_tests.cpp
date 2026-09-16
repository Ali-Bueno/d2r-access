#include "quest_progress.h"
#include "test_support.h"

#include <map>

using namespace D2RAccess;
using namespace D2RAccess::QuestProgress;
namespace {
const Reading first{"El Cubil del Mal", "Monstruos restantes: 3"};
const Reading second{"El Cubil del Mal", "Monstruos restantes: 2"};
const Reading last{"El Cubil del Mal", "Falta un monstruo."};

void Baseline(Reader& reader, const Reading& reading = first) {
	Check(reader.Update(reading, 0, true).kind == Kind::None, "initial objective is silent");
	Check(reader.Update(reading, SettleMs, true).kind == Kind::None, "settled initial objective is a baseline");
}

void Identification() {
	const std::map<std::string, std::string, std::less<>> strings{
		{"qstsa1q1", first.quest}, {"qstsa1q12", "Mata a todos los monstruos."},
		{"qstsa1q14", "Monstruos restantes: %d"}, {"qstsa1q140", last.objective},
		{"qstsa1q15", "Vuelve con Akara."}, {"qstsa1q5", "La Torre Olvidada"},
		{"qstsa1q51", "Busca la torre."}};
	const auto resolve = [&](std::string_view key) {
		const auto found = strings.find(key);
		return found == strings.end() ? std::string() : found->second;
	};
	Check(Identify("Misiones", first.quest, first.objective, resolve) == first, "stock short title and formatted counter identify the quest");
	Check(Identify(first.quest, first.objective, "", resolve) == first, "a title widget can also identify the quest");
	Check(Identify("", first.quest, last.objective, resolve) == last, "singular counter uses the game's own sentence");
	Check(Identify("", first.quest, "Monstruos restantes: %d", resolve).quest.empty(), "raw format strings are rejected");
	Check(Identify("", first.quest, "Monstruos restantes: perro", resolve).quest.empty(), "a counter must carry digits");
	Check(Identify("", first.quest, "Monstruos restantes: -1", resolve).quest.empty(), "negative counters are rejected");
	Check(Identify("", first.quest, "Monstruos restantes: ", resolve).quest.empty(), "missing counter values are rejected");
	Check(Identify("", first.quest, "Busca la torre.", resolve).quest.empty(), "a stale title paired with another quest's objective is rejected");
	Check(Identify("", "Misión desconocida", first.objective, resolve).quest.empty(), "unknown quest identities are rejected");
	Check(Identify("", first.quest, "Un objetivo inventado", resolve).quest.empty(), "unknown objective text is rejected");
	Check(Identify("", first.quest, first.objective, {}).quest.empty(), "unavailable localization is tolerated");
	const auto multiline = Identify("", first.quest, "Mata a todos los monstruos.\r\nMonstruos restantes: 2", resolve);
	Check(multiline.objective == "Mata a todos los monstruos. Monstruos restantes: 2", "each line belongs to the identified quest");
	Check(Identify("", first.quest, "Mata a todos los monstruos.\nBusca la torre.", resolve).quest.empty(), "a mixed-quest body is rejected");
	Check(Identify("", "\xC3\xBF" "c1El Cubil del Mal", "Monstruos restantes: 3", resolve) == first, "stock color codes do not change identity");
}

void TextChanges() {
	Reader reader;
	Baseline(reader);
	Check(reader.Update(second, SettleMs * 2, true).kind == Kind::None, "partial writes settle before speaking");
	const auto result = reader.Update(second, SettleMs * 3, true);
	Check(result.kind == Kind::Objective && result.quest == first.quest && result.objective == second.objective, "a fresh objective is spoken with its own quest");
	Check(reader.Update(second, AlertDelayMs * 2, true).kind == Kind::None, "steady counters never repeat");
	reader.Navigate();
	Check(reader.Update(last, AlertDelayMs * 3, true).kind == Kind::None, "selecting a quest does not indicate progress");
	Check(reader.Update(last, AlertDelayMs * 4, true).kind == Kind::None, "navigation establishes a new baseline");
	Reading other{"La Torre Olvidada", "Busca la torre."};
	Check(reader.Update(other, AlertDelayMs * 5, true).kind == Kind::None, "switching quest identity is silent");
	Check(reader.Update(other, AlertDelayMs * 6, true).kind == Kind::None, "another quest is not mistaken for a previous quest's update");
	reader.Reset(); Baseline(reader);
	reader.Update({}, SettleMs * 2, true);
	reader.Update(second, SettleMs * 3, true);
	Check(reader.Update(second, SettleMs * 4, true).kind == Kind::None, "unloading and recreating a panel only rebaseline");
	reader.Reset(); Reading opened = first; opened.open = true; Baseline(reader, opened);
	opened.objective = second.objective;
	reader.Update(opened, SettleMs * 2, true);
	Check(reader.Update(opened, SettleMs * 3, true).kind == Kind::None, "the normal UI reader owns the open journal");
	reader.Update(second, SettleMs * 4, true);
	Check(reader.Update(second, SettleMs * 5, true).kind == Kind::None, "closing the journal is silent");
}

void Alerts() {
	Reader reader;
	reader.Alert(0);
	Check(reader.Update({}, AlertDelayMs - 1, true).kind == Kind::None, "generic fallback waits for deferred text");
	reader.Alert(SettleMs);
	Check(reader.Update({}, AlertDelayMs, true).kind == Kind::Updated, "duplicate alerts do not postpone an unavailable-panel notice");
	Check(reader.Update({}, DuplicateWindowMs, true).kind == Kind::None, "the same burst is spoken once");
	reader.Alert(DuplicateWindowMs);
	Check(reader.Update({}, DuplicateWindowMs + AlertDelayMs, true).kind == Kind::Updated, "a later update can repeat the generic wording");
	reader.Reset(); Baseline(reader);
	reader.Alert(SettleMs * 2);
	Check(reader.Update(first, SettleMs * 2 + AlertDelayMs, true).kind == Kind::Updated, "an alert never reads a stale objective");
	reader.Reset(); Baseline(reader); reader.Alert(SettleMs * 2);
	reader.Update(second, SettleMs * 2, true);
	Check(reader.Update(second, SettleMs * 3, true).kind == Kind::Objective, "updated text replaces the generic alert");
	Check(reader.Update(second, SettleMs * 2 + AlertDelayMs, true).kind == Kind::None, "specific and generic announcements do not duplicate");
	reader.Reset(); reader.Alert(0); reader.Completed(1, 0, SettleMs);
	Check(reader.Update({}, AlertDelayMs, true).kind == Kind::GoalDone, "SDK goal completion supersedes the same update burst");
	reader.Completed(1, 0, AlertDelayMs);
	Check(reader.Update({}, AlertDelayMs * 2, true).kind == Kind::None, "duplicate completion records stay silent");
	reader.Completed(1, 1, AlertDelayMs * 2);
	Check(reader.Update({}, AlertDelayMs * 3, true).kind == Kind::GoalDone, "difficulty is part of completion identity");
	reader.Reset(); reader.Completed(1, 0, 0);
	Check(reader.Update({}, AlertDelayMs, true).kind == Kind::GoalDone, "another session can report that quest again");
}

void DeferredSpeech() {
	Reader reader;
	reader.Alert(0);
	Check(reader.Update({}, AlertDelayMs, false).kind == Kind::None, "dialogue, UI and focus gates hold announcements");
	Check(reader.Update({}, AlertDelayMs * 3, true).kind == Kind::Updated, "an alert is retained until gameplay resumes");
	reader.Reset(); Baseline(reader);
	reader.Update(second, SettleMs * 2, false);
	reader.Update(second, SettleMs * 3, false);
	reader.Update(last, SettleMs * 4, false);
	Check(reader.Update(last, SettleMs * 5, true).objective == last.objective, "deferred counters coalesce to the latest value");
	Check(reader.Update(last, SettleMs * 6, true).kind == Kind::None, "old counters are not queued behind the latest value");
	reader.Alert(AlertDelayMs * 2); reader.Reset();
	Check(reader.Update({}, AlertDelayMs * 4, true).kind == Kind::None, "disable, game leave and locale reset discard pending speech");
}
}

int main() {
	Identification(); TextChanges(); Alerts(); DeferredSpeech();
	Passed();
}
