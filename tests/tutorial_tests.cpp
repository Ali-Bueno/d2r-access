#include "tutorial.h"
#include "tutorial_text.h"
#include "test_support.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace D2RAccess::Tutorial;
namespace Text = D2RAccess::TutorialText;
namespace {
void Steps() {
	Guide guide;
	Check(guide.Finished() && !guide.Done(~Signals{0}), "a guide not started has nothing to finish");
	guide.Start(Step::Vitals);
	Check(!guide.Done(0), "the first step waits for its action");
	guide.Notice(Bit(Signal::Wall));
	Check(!guide.Done(0), "another step's action does not finish this one");
	guide.Notice(Bit(Signal::Vitals));
	Check(guide.Done(0), "the life and mana readout finishes the first step");
	guide.Advance();
	Check(guide.Current() == Step::Radar && !guide.Done(0),
		"an action done before its step was current is asked for again: each step teaches its own");
	guide.Notice(Bit(Signal::Wall));
	Check(guide.Done(0), "a wall heard on the radar step finishes it");
	guide.Advance();
	Check(guide.Current() == Step::Sounds && !guide.Done(Bit(Signal::Wall)),
		"the world's sounds come after the radar's, and another wall does not end their lesson");
	guide.Notice(Bit(Signal::HeardSounds));
	Check(guide.Done(0), "the lesson ends once its sounds have played");
	guide.Advance();
	Check(guide.Current() == Step::Scanner, "then the scanner");
}

void States() {
	Guide guide;
	guide.Start(Step::LeaveTown);
	Check(!guide.Done(Bit(Signal::InTown)), "being in the camp does not leave it");
	Check(guide.Done(Bit(Signal::InBloodMoor)), "being in the Blood Moor finishes leaving the camp");
	guide.Advance();
	guide.Notice(Bit(Signal::RoomWays));
	Check(!guide.Done(0), "outdoors the ways out of a room are not asked: they say nothing there (a tester)");
	guide.Notice(Bit(Signal::Terrain));
	Check(guide.Done(0), "the terrain overview finishes the surroundings step");
	guide.Advance();
	guide.Notice(Bit(Signal::Kill));
	Check(guide.Current() == Step::Fight && guide.Done(0), "a monster killed finishes the fight, gold or not (a tester)");
	guide.Advance();
	Check(guide.Done(Bit(Signal::InDen)), "being in the Den finds it");
	guide.Advance();
	Check(guide.Current() == Step::DenRooms && !guide.Done(Bit(Signal::InDen)), "inside, the rooms' ways out are asked for");
	guide.Notice(Bit(Signal::RoomWays));
	Check(guide.Done(0), "asking for them finishes the step");
	guide.Advance();
	Check(guide.Current() == Step::ClearDen && guide.Done(Bit(Signal::DenCleared)), "then the Den is cleared");
	guide.Start(Step::Talk);
	Check(guide.Done(Bit(Signal::TalkedToAkara)), "a quest Akara already gave passes the talking step");
	guide.Start(Step::Scanner);
	guide.Notice(Bit(Signal::ScannedAkara));
	Check(!guide.Done(0), "the scanner step asks for Warriv, the NPC nearest the start, not Akara");
	guide.Notice(Bit(Signal::ScannedWarriv));
	Check(guide.Done(0), "Warriv named finishes the scanner step");
	guide.Advance();
	Check(guide.Current() == Step::MeetWarriv && !guide.Done(Bit(Signal::TalkedToAkara)),
		"talking to Warriv comes next, and another NPC's menu does not finish it");
	Check(guide.Done(Bit(Signal::TalkedToWarriv)), "Warriv's menu finishes it");
	guide.Advance();
	Check(guide.Current() == Step::FindAkara, "then Akara, found with the scanner");
	guide.Start(Step::Reward);
	Check(!guide.Done(Bit(Signal::InTown) | Bit(Signal::DenCleared)), "back in the camp is not yet the reward");
	Check(guide.Done(Bit(Signal::DenDone)), "the quest done finishes the reward step");
	guide.Advance();
	Check(guide.Current() == Step::Waypoint && !guide.Done(Bit(Signal::InTown) | Bit(Signal::DenDone)),
		"then a waypoint, which nothing done so far finishes");
	guide.Notice(Bit(Signal::Waypoint));
	Check(guide.Done(0), "a waypoint's menu opened outside the camp finishes it");
	guide.Advance();
	Check(guide.Finished() && !guide.Done(~Signals{0}), "past the last step nothing remains");
	guide.Advance();
	Check(guide.Finished(), "advancing past the end stays at the end");
}

void AheadOfTheTutorial() {
	Guide guide;
	guide.Start(Step::Fight);
	Check(!guide.Passed(Bit(Signal::InDen)), "in the Den a fight is still to be had");
	Check(guide.Passed(Bit(Signal::DenCleared)) && guide.Done(Bit(Signal::DenCleared)),
		"the Den cleared passes the steps on the way to it, so the tutorial is never left behind");
	for (auto step : {Step::LeaveTown, Step::Surroundings, Step::FindDen, Step::DenRooms}) {
		guide.Start(step);
		Check(guide.Passed(Bit(Signal::DenDone)), "the quest done passes them too");
	}
	guide.Start(Step::ReturnToTown);
	Check(!guide.Passed(Bit(Signal::DenCleared)) && guide.Passed(Bit(Signal::DenDone)), "the reward taken was back in the camp");
	guide.Start(Step::Vitals);
	Check(!guide.Passed(~Signals{0}), "the camp's lessons are never passed over");
	guide.Start(Step::Waypoint);
	Check(!guide.Passed(~Signals{0} & ~Bit(Signal::Waypoint)), "nor the waypoint");
}

void OlderRecords() {
	Check(FromVersion1(0) == Step::Vitals && FromVersion1(12) == Step::FindDen, "the steps before the Den keep their number");
	Check(FromVersion1(13) == Step::DenRooms, "in the Den, it begins again with its rooms");
	Check(FromVersion1(14) == Step::ReturnToTown && FromVersion1(15) == Step::Reward, "the way back and the reward move on one");
	Check(FromVersion1(16) == Step::Finished, "a tutorial finished stays finished: the waypoint is not asked of it");
	Check(FromVersion1(-1) == Step::Finished && FromVersion1(40) == Step::Finished, "nonsense counts as finished");
}

void Sounds() {
	using Cue = Audition::Cue;
	Audition lesson;
	Check(lesson.Idle() && !lesson.Again(), "an audition not started has nothing to play again");
	lesson.Start({1000, 2000}, 0, 500, false);
	auto due = lesson.Tick(0);
	Check(due.cue == Cue::Name && due.index == 0, "the first name is due at once");
	Check(!lesson.Again(), "nothing to play again before the first sound");
	Check(lesson.Tick(999).cue == Cue::None, "its sound waits for the name to be read");
	due = lesson.Tick(1000);
	Check(due.cue == Cue::Sound && due.index == 0, "then the sound");
	Check(lesson.Tick(1499).cue == Cue::None && lesson.Tick(1500).cue == Cue::None, "a gap after it");
	due = lesson.Tick(1500);
	Check(due.cue == Cue::Name && due.index == 1, "then the next name");
	const auto again = lesson.Again();
	Check(again && *again == 0 && lesson.Holding(), "again plays the last sound heard and holds the lesson (a tester)");
	Check(lesson.Tick(60000).cue == Cue::None, "held, nothing comes however long it waits");
	lesson.Next(60000);
	due = lesson.Tick(60000);
	Check(due.cue == Cue::Name && due.index == 1, "on, the next name is said again, as the sound it named was cut");
	due = lesson.Tick(62000);
	Check(due.cue == Cue::Sound && due.index == 1, "and its sound follows");
	Check(lesson.Tick(62499).cue == Cue::None && !lesson.Done(), "after the last sound a gap still lets it be played again");
	lesson.Tick(62500);
	Check(lesson.Done(), "then the audition is done");

	lesson.Start({100}, 0, 500, true);
	lesson.Tick(0);
	lesson.Tick(100);
	Check(lesson.Holding() && !lesson.Done(), "waiting for the player, each sound holds");
	Check(lesson.Again() && lesson.Again() && lesson.Holding(), "again as often as wished");
	lesson.Next(200);
	lesson.Tick(200);
	Check(lesson.Done(), "next after the last sound ends it");
	lesson.Next(300);
	Check(lesson.Done() && !lesson.Again(), "a done audition takes no more commands");
	lesson.Reset();
	Check(lesson.Idle(), "reset, it is idle");
	lesson.Start({}, 0, 500, false);
	Check(lesson.Done(), "no sounds, nothing to wait for");
}

void EveryStepWaitsForSomething() {
	for (size_t step = 0; step < StepCount; ++step)
		Check(Needs(static_cast<Step>(step)) != 0, "every step but the farewell waits for an action or a state");
	Check(Needs(Step::Finished) == 0, "the farewell waits for nothing");
}

void Mailbox() {
	(void)TakePosted();
	Post(Signal::Vitals);
	Post(Signal::Kill);
	Post(Signal::Vitals);
	Check(TakePosted() == (Bit(Signal::Vitals) | Bit(Signal::Kill)), "posted signals arrive together, once each");
	Check(TakePosted() == 0, "taking empties the mailbox");
}

void Browsing() {
	History history;
	Check(!history.Previous() && !history.Next() && !history.First(), "an empty history has nothing to read");
	history.Add("one");
	history.Add("two");
	history.Add("three");
	auto reading = history.Next();
	Check(reading && reading->text == "three" && reading->edge == History::Edge::Last,
		"a new message leaves the cursor on it, and next says it is the last");
	reading = history.Previous();
	Check(reading && reading->text == "two" && reading->edge == History::Edge::None && reading->at == 1,
		"previous goes back one, and says where it is, for the sound a message carries");
	reading = history.First();
	Check(reading && reading->text == "one" && reading->edge == History::Edge::None, "first goes to the beginning");
	reading = history.Previous();
	Check(reading && reading->text == "one" && reading->edge == History::Edge::First, "before the first, it says so and stays");
	history.Add("four");
	reading = history.Previous();
	Check(reading && reading->text == "three", "a new message moves the cursor to the end");
	history.Clear();
	Check(history.Empty() && !history.First(), "cleared, it is empty");
}

// Every message beside the steps.
std::vector<const Text::Lines*> Lessons(const Text::Texts& t) {
	return {&t.radarTask, &t.worldTask, &t.leaveTask, &t.auditionAgain, &t.auditionWait, &t.portalButton, &t.gold, &t.shop,
		&t.levelUp, &t.levelUpSheet, &t.levelUpSwitch, &t.levelUpTree, &t.levelUpTreeLater, &t.levelUpBind, &t.kit, &t.kitOpen,
		&t.kitWorn, &t.kitDone, &t.levelUpDone};
}

// Every string of a table in one order, to hold each translation against the English.
std::vector<const char*> All(const Text::Texts& t) {
	std::vector<const char*> all;
	const auto add = [&all](const Text::Lines& lines) { all.push_back(lines.pad); all.push_back(lines.keys); };
	add(t.welcome);
	for (const auto& lines : t.steps) add(lines);
	for (const auto* lines : Lessons(t)) add(*lines);
	all.insert(all.end(), std::begin(t.cues), std::end(t.cues));
	all.insert(all.end(), std::begin(t.worldCues), std::end(t.worldCues));
	all.insert(all.end(), std::begin(t.enemyCues), std::end(t.enemyCues));
	all.insert(all.end(), {t.routeStick, t.routeButton, t.firstMessage, t.lastMessage, t.den, t.moor, t.plains, t.portalTome,
		t.identifyTome, t.generalSkills});
	return all;
}

// The {tokens} a text holds, sorted: the buttons and places the game fills in.
std::vector<std::string> TokensOf(const char* text) {
	std::vector<std::string> tokens;
	const std::string_view view(text ? text : "");
	for (size_t at = view.find('{'); at != std::string_view::npos; at = view.find('{', at + 1)) {
		const size_t end = view.find('}', at);
		if (end == std::string_view::npos) { tokens.emplace_back(view.substr(at)); break; }
		tokens.emplace_back(view.substr(at, end - at + 1));
	}
	std::sort(tokens.begin(), tokens.end());
	return tokens;
}

void Words() {
	using D2RAccess::Strings::Language;
	const auto& english = Text::For(Language::enUS);
	const auto& spanish = Text::For(Language::esES);
	Check(&Text::For(Language::esMX) == &spanish, "both Spanish locales read the Spanish");
	const auto reference = All(english);
	for (size_t language = 0; language < D2RAccess::Strings::LanguageCount; ++language) {
		const auto& texts = Text::For(static_cast<Language>(language));
		Check(language == static_cast<size_t>(Language::enUS) || &texts != &english, "every language has its own words");
		const auto strings = All(texts);
		for (size_t i = 0; i < strings.size(); ++i) {
			Check(strings[i] && *strings[i], "every text is translated");
			Check(TokensOf(strings[i]) == TokensOf(reference[i]), "a translation keeps the English text's buttons and places");
		}
	}
	for (const auto* texts : {&english, &spanish}) {
		for (const auto& lines : texts->steps)
			Check(lines.pad && lines.keys && *lines.pad && *lines.keys, "every step has words for the pad and the keyboard");
		for (const auto* cue : texts->cues) Check(cue && *cue, "every radar sound has a name");
		for (const auto* cue : texts->worldCues) Check(cue && *cue, "every sound of the world's lesson has a name");
		for (const auto* cue : texts->enemyCues) Check(cue && *cue, "every monster's sound has a name");
		for (const auto* lines : Lessons(*texts))
			Check(lines->pad && *lines->pad && lines->keys && *lines->keys, "every lesson has words for the pad and the keyboard");
	}

	const auto names = [](uint32_t glyph) { return "<" + std::to_string(glyph - 0xE000) + ">"; };
	const Text::Names none;
	Check(Text::Render("{view} and {x}", english, false, none, names) == "<6> and <17>", "pad tokens become the glyphs' names");
	Check(Text::Render("press {route}", english, false, none, names) == "press <6> and <9> down",
		"the route's chord is BACK and right stick down");
	Check(Text::Render("press {route}", english, true, none, names) == "press <6> and <15>",
		"with the route on A, the chord follows the switch");
	Check(Text::Render("{a}{a} K", english, false, none, names) == "<15><15> K", "every occurrence is replaced, plain text kept");
	const Text::Names mexico{"Guarida del Mal", "Páramo Sangriento", "Llanuras Heladas", "Tomo de portal al poblado"};
	Check(Text::Render("{den}, {moor}", spanish, false, mexico, names) == "Guarida del Mal, Páramo Sangriento",
		"the places take the game's own names, which differ between Spain and Mexico");
	Check(Text::Render("{den}, {moor}", spanish, false, none, names) == "Cubil del Mal, Landa de Sangre",
		"unread, they take the text's own");
	Check(Text::Render("{plains}: {portaltome}, {idtome}", spanish, false, mexico, names)
			== "Llanuras Heladas: Tomo de portal al poblado, Tomo de identificación",
		"the tomes and the next area by the game's names, the text's own where unread");
	const Text::Names bound{.portal = "LT X"};
	Check(Text::Render("{portal}", english, false, bound, names) == "LT X", "the town portal's button as the bar has it");
	Check(Text::Render("{menu}", english, false, none, names) == "<5>", "the Menu button is the level-up prompt's");
	for (const auto* texts : {&english, &spanish})
		for (const auto& lines : texts->steps) {
			const auto pad = Text::Render(lines.pad, *texts, false, mexico, names);
			const auto keys = Text::Render(lines.keys, *texts, false, mexico, names);
			Check(pad.find('{') == std::string::npos && keys.find('{') == std::string::npos, "no step leaves a token unreplaced");
			Check(keys.find('<') == std::string::npos, "the keyboard's words carry no pad buttons");
		}
	for (const auto* texts : {&english, &spanish})
		for (const auto* lines : Lessons(*texts))
			for (const auto* text : {lines->pad, lines->keys})
				Check(Text::Render(text, *texts, false, mexico, names).find('{') == std::string::npos,
					"no lesson leaves a token unreplaced");
}
}

int main() {
	Steps();
	States();
	AheadOfTheTutorial();
	OlderRecords();
	Sounds();
	EveryStepWaitsForSomething();
	Mailbox();
	Browsing();
	Words();
	Passed();
}
