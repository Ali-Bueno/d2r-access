#include "test_support.h"
#include "speech_strings.h"
#include "trade.h"

using namespace D2RAccess;
using Trade::Event;
using Events = std::vector<Event>;

namespace {
Trade::State Active() {
	Trade::State state;
	state.active = true;
	state.partner = 7;
	return state;
}

void Baseline() {
	Trade::Tracker tracker;
	auto state = Active();
	state.partnerGold = 500;
	Check(tracker.Update(state, false, true).empty(), "a trade already open when reading starts is the baseline");
	Trade::State idle, pending;
	pending.pending = true;
	tracker.Reset();
	Check(tracker.Update(idle, false, true).empty() && tracker.Update(pending, false, true).empty(),
		"a request is read from the game's own prompt, not announced here");
	Check(tracker.Update(Active(), false, true).empty(), "opening the trade is told by its window, not here");
}

void PartnerChanges() {
	Trade::Tracker tracker;
	tracker.Update(Trade::State{}, false, true);
	auto state = Active();
	tracker.Update(state, false, true);
	state.partnerItems = 2;
	state.partnerGold = 1000;
	Check(tracker.Update(state, false, true) == Events{Event::PartnerItems, Event::PartnerGold},
		"the partner's items and gold are told, items first");
	Check(tracker.Update(state, false, true).empty(), "an unchanged offer says nothing");
	state.partnerAccepted = true;
	Check(tracker.Update(state, false, true) == Events{Event::PartnerAccepted}, "the partner accepting is told");
	state.partnerAccepted = false;
	state.weAccepted = true;
	Check(tracker.Update(state, false, true) == Events{Event::PartnerWithdrew, Event::WeAccepted},
		"withdrawn and given acceptance are both told");
	state.partnerGold = 900;
	state.weAccepted = false;
	Check(tracker.Update(state, false, true) == Events{Event::PartnerGold, Event::WeWithdrew},
		"a changed offer resets this player's acceptance, and that is told");
}

void Endings() {
	Trade::Tracker tracker;
	tracker.Update(Trade::State{}, false, true);
	tracker.Update(Active(), false, true);
	Check(tracker.Update(Active(), true, true).empty(), "the completion reply alone says nothing");
	Check(tracker.Update(Trade::State{}, false, true) == Events{Event::Completed}, "the window closing after it is a completed trade");
	tracker.Update(Active(), false, true);
	Check(tracker.Update(Trade::State{}, false, true) == Events{Event::Cancelled}, "closing without the reply is a cancelled trade");
	tracker.Update(Active(), false, false);
	Check(tracker.Update(Trade::State{}, false, false) == Events{Event::Ended}, "without the hook the end is not guessed");
	tracker.Update(Trade::State{}, true, true);
	tracker.Update(Active(), false, true);
	Check(tracker.Update(Trade::State{}, false, true) == Events{Event::Cancelled}, "an old completion is not the next trade's");
}

void Phrases() {
	Strings::Select(Strings::Language::esES);
	auto state = Active();
	state.partnerGold = 1000;
	state.partnerItems = 3;
	Check(Trade::Phrase(Event::PartnerGold, state, "canaria") == "canaria ofrece 1000 de oro", "gold is named with the partner");
	Check(Trade::Phrase(Event::PartnerItems, state, "canaria") == "Objetos de canaria: 3", "items are counted");
	Check(Trade::Phrase(Event::PartnerAccepted, state, "") == "el otro jugador ha aceptado el trato", "an unknown partner is still named");
	Check(Trade::Phrase(Event::Completed, state, "canaria") == "Comercio completado", "the ending is told");
	Check(Trade::NamedPrompt("Aceptar comerciar", "Aceptar comerciar", "Pidiendo a otro jugador que comercie", "canaria")
		== "canaria quiere comerciar contigo", "the request prompt names who asked");
	Check(Trade::NamedPrompt("Pidiendo a otro jugador que comercie", "Aceptar comerciar", "Pidiendo a otro jugador que comercie", "canaria")
		== "Pidiendo a canaria que comercie", "the waiting prompt names who was asked");
	Check(Trade::NamedPrompt("Hablar", "Aceptar comerciar", "Pidiendo a otro jugador que comercie", "canaria").empty(),
		"other menu lines are left alone");
	Check(Trade::NamedPrompt("Aceptar comerciar", "Aceptar comerciar", "Pidiendo", "").empty(), "no name, no change");
	Check(Trade::SpacedNameSuffix("canariano está disponible.", "no está disponible.") == "canaria no está disponible.",
		"the space the esES line drops after a name is put back");
	Check(Trade::SpacedNameSuffix("canaria no está disponible.", "no está disponible.") == "canaria no está disponible.",
		"a spaced line is left alone");
	Check(Trade::SpacedNameSuffix("canaria is busy", " is busy") == "canaria is busy", "a suffix with its own space is left alone");
}
}

int main() {
	Baseline(); PartnerChanges(); Endings(); Phrases();
	Passed();
}
