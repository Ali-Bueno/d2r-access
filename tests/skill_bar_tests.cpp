#include "skill_bar.h"
#include "controller_text.h"
#include "speech_strings.h"
#include "test_support.h"
#include <string>
#include <vector>

namespace {
using namespace D2RAccess;
using SkillBar::Action;
using SkillBar::PeekHoldMs;

// Feeds one tick; `held`/`other` are LT and any other button, `playing` the InPlay gate.
Action Tick(SkillBar::PeekGate& gate, uint64_t now, bool held, bool other = false, bool playing = true) {
	return gate.Update(now, playing, held, other);
}

void HoldingAloneReadsOnceAndReleaseCuts() {
	SkillBar::PeekGate gate;
	Check(Tick(gate, 1000, false) == Action::None, "nothing held says nothing");
	Check(Tick(gate, 1000, true) == Action::None, "pressing LT does not read yet");
	Check(Tick(gate, 1000 + PeekHoldMs - 1, true) == Action::None, "a hold just short of the delay stays silent");
	Check(Tick(gate, 1000 + PeekHoldMs, true) == Action::Speak, "a hold of the full delay reads the bar");
	Check(Tick(gate, 1000 + PeekHoldMs * 3, true) == Action::None, "a longer hold reads only once");
	Check(Tick(gate, 1000 + PeekHoldMs * 4, false) == Action::Stop, "releasing LT cuts the reading");
	Check(Tick(gate, 1000 + PeekHoldMs * 5, false) == Action::None, "a release is cut only once");
	Check(Tick(gate, 9000, true) == Action::None && Tick(gate, 9000 + PeekHoldMs, true) == Action::Speak,
		"the next hold reads again");
}

void CombosNeverRead() {
	SkillBar::PeekGate gate;
	Tick(gate, 0, true);
	Check(Tick(gate, 100, true, true) == Action::None, "LT plus a button casts, it does not read");
	Check(Tick(gate, 100 + PeekHoldMs * 2, true) == Action::None, "letting go of the button keeps that hold silent");
	Check(Tick(gate, 100 + PeekHoldMs * 3, false) == Action::None, "releasing a silent hold stops nothing");

	SkillBar::PeekGate pressedFirst;
	Check(Tick(pressedFirst, 0, true, true) == Action::None, "a button already down when LT arrives");
	Check(Tick(pressedFirst, PeekHoldMs * 2, true) == Action::None, "makes that whole hold a combo");
}

void ComboAfterListeningCutsTheReading() {
	SkillBar::PeekGate gate;
	Tick(gate, 0, true);
	Check(Tick(gate, PeekHoldMs, true) == Action::Speak, "the bar is read");
	Check(Tick(gate, PeekHoldMs + 200, true, true) == Action::Stop, "casting from the bar cuts the reading");
	Check(Tick(gate, PeekHoldMs + 300, true) == Action::None, "and does not read it again");
	Check(Tick(gate, PeekHoldMs + 400, false) == Action::None, "nor cut anything on release");
}

void OnlyInPlay() {
	SkillBar::PeekGate menu;
	Tick(menu, 0, true, false, false);
	Check(Tick(menu, PeekHoldMs * 2, true, false, true) == Action::None, "a hold that began in a menu stays silent");

	SkillBar::PeekGate opened;
	Tick(opened, 0, true);
	Check(Tick(opened, PeekHoldMs / 2, true, false, false) == Action::None, "a menu opening during the delay");
	Check(Tick(opened, PeekHoldMs * 2, true) == Action::None, "cancels that hold");

	SkillBar::PeekGate speaking;
	Tick(speaking, 0, true);
	Tick(speaking, PeekHoldMs, true);
	Check(Tick(speaking, PeekHoldMs + 100, true, false, false) == Action::None,
		"a menu opening during the reading leaves speech to the menu");
	Check(Tick(speaking, PeekHoldMs + 200, false) == Action::None, "and the release does not cut the menu's speech");
}

SkillBar::Slot Bound(std::string button, std::string skill, int quantity = 0) { return {std::move(button), true, std::move(skill), quantity}; }
SkillBar::Slot Unbound(std::string button) { return {std::move(button), false, {}, 0}; }

void ComposesTheBarInHudOrder() {
	Strings::Select(Strings::Language::esES);
	const std::vector<SkillBar::Slot> slots{Bound("A", "Tomo de portal a la ciudad", 12), Unbound("X"),
		Bound("B", "Grito"), Bound("Y", ""), Bound("", "Sin botón"), Unbound("RT")};
	Check(SkillBar::Compose(slots) == "A: Tomo de portal a la ciudad, 12. X: Vacío. B: Grito. RT: Vacío",
		"bound, empty and counted slots in order; unreadable ones skipped");
	Check(SkillBar::Compose({}).empty(), "no slots, nothing to say");
	Check(SkillBar::Compose(std::vector{Unbound("")}).empty(), "a slot without a button is never guessed");
	Strings::Select(Strings::Language::frFR);
	Check(SkillBar::Compose(std::vector{Bound("RB", "Téléportation", 3)}) == "RB : Téléportation, 3", "the locale's own punctuation");
}

void FirstButtonNamesTheHelpLine() {
	Strings::Select(Strings::Language::esES);
	// ui-controller.json ControllerHelpSkill1 ("  Skill 1") and ControllerHelpToggleSkills.
	Check(ControllerText::FirstButton("\xEE\x80\x8F  Habilidad 1") == "A", "the help line's control");
	Check(ControllerText::FirstButton("Alternar habilidades  \xEE\x80\x8D") == "LT", "the control after the caption");
	Check(ControllerText::FirstButton("Habilidad 1").empty(), "no control symbol, no button");
	Check(ControllerText::FirstButton("\xEE\x80\x96 \xEE\x80\x8C") == "RB", "an unknown symbol is passed over");
	Check(ControllerText::FirstButton("\xEE\x80").empty(), "a truncated symbol is not read");
}
}

int main() {
	HoldingAloneReadsOnceAndReleaseCuts();
	CombosNeverRead();
	ComboAfterListeningCutsTheReading();
	OnlyInPlay();
	ComposesTheBarInHudOrder();
	FirstButtonNamesTheHelpLine();
	Passed();
}
