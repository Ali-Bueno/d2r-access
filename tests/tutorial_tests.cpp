#include "tutorial.h"
#include "tutorial_text.h"
#include "test_support.h"

#include <string>

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
	Check(!guide.Done(Bit(Signal::RoomWays)), "a step that needs two actions waits for both");
	guide.Notice(Bit(Signal::RoomWays));
	Check(!guide.Done(0), "one of two is not enough");
	guide.Notice(Bit(Signal::Terrain));
	Check(guide.Done(0), "both asked finish the surroundings step");
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
	Check(guide.Finished() && !guide.Done(~Signals{0}), "past the last step nothing remains");
	guide.Advance();
	Check(guide.Finished(), "advancing past the end stays at the end");
}

void EveryStepWaitsForSomething() {
	for (size_t step = 0; step < StepCount; ++step)
		Check(Needs(static_cast<Step>(step)) != 0, "every step but the farewell waits for an action or a state");
	Check(Needs(Step::Finished) == 0, "the farewell waits for nothing");
}

void Mailbox() {
	(void)TakePosted();
	Post(Signal::Vitals);
	Post(Signal::Gold);
	Post(Signal::Vitals);
	Check(TakePosted() == (Bit(Signal::Vitals) | Bit(Signal::Gold)), "posted signals arrive together, once each");
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
	Check(reading && reading->text == "two" && reading->edge == History::Edge::None, "previous goes back one");
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

void Words() {
	const auto& english = Text::For(D2RAccess::Strings::Language::enUS);
	const auto& spanish = Text::For(D2RAccess::Strings::Language::esES);
	Check(&Text::For(D2RAccess::Strings::Language::esMX) == &spanish, "both Spanish locales read the Spanish");
	Check(&Text::For(D2RAccess::Strings::Language::deDE) == &english, "a language without its own words reads the English");
	for (const auto* texts : {&english, &spanish}) {
		for (const auto& lines : texts->steps)
			Check(lines.pad && lines.keys && *lines.pad && *lines.keys, "every step has words for the pad and the keyboard");
		for (const auto* cue : texts->cues) Check(cue && *cue, "every radar sound has a name");
		for (const auto* cue : texts->worldCues) Check(cue && *cue, "every sound of the world's lesson has a name");
		Check(texts->worldTask.pad && *texts->worldTask.pad && texts->worldTask.keys && *texts->worldTask.keys,
			"the world's lesson says what comes next");
		for (const auto* lines : {&texts->levelUp, &texts->levelUpSheet, &texts->levelUpSwitch, &texts->levelUpTree,
			&texts->levelUpTreeLater, &texts->levelUpBind, &texts->levelUpDone, &texts->kit, &texts->kitOpen, &texts->kitWorn, &texts->kitDone})
			Check(lines->pad && *lines->pad && lines->keys && *lines->keys, "the level-up lesson has words for the pad and the keyboard");
	}

	const auto names = [](uint32_t glyph) { return "<" + std::to_string(glyph - 0xE000) + ">"; };
	const Text::Places none;
	Check(Text::Render("{view} and {x}", english, false, none, names) == "<6> and <17>", "pad tokens become the glyphs' names");
	Check(Text::Render("press {route}", english, false, none, names) == "press <6> and <9> down",
		"the route's chord is BACK and right stick down");
	Check(Text::Render("press {route}", english, true, none, names) == "press <6> and <15>",
		"with the route on A, the chord follows the switch");
	Check(Text::Render("{a}{a} K", english, false, none, names) == "<15><15> K", "every occurrence is replaced, plain text kept");
	const Text::Places mexico{"Guarida del Mal", "Páramo Sangriento"};
	Check(Text::Render("{den}, {moor}", spanish, false, mexico, names) == "Guarida del Mal, Páramo Sangriento",
		"the places take the game's own names, which differ between Spain and Mexico");
	Check(Text::Render("{den}, {moor}", spanish, false, none, names) == "Cubil del Mal, Landa de Sangre",
		"unread, they take the text's own");
	Check(Text::Render("{menu}", english, false, none, names) == "<5>", "the Menu button is the level-up prompt's");
	for (const auto* texts : {&english, &spanish})
		for (const auto& lines : texts->steps) {
			const auto pad = Text::Render(lines.pad, *texts, false, mexico, names);
			const auto keys = Text::Render(lines.keys, *texts, false, mexico, names);
			Check(pad.find('{') == std::string::npos && keys.find('{') == std::string::npos, "no step leaves a token unreplaced");
			Check(keys.find('<') == std::string::npos, "the keyboard's words carry no pad buttons");
		}
}
}

int main() {
	Steps();
	States();
	EveryStepWaitsForSomething();
	Mailbox();
	Browsing();
	Words();
	Passed();
}
