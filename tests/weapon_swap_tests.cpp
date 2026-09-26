#include "test_support.h"
#include "speech_strings.h"
#include "weapon_swap.h"

using namespace D2RAccess;
using WeaponSwap::Hands;

namespace {
const Hands sword{10, 11}, bow{20, 21}, empty{};

void Baseline() {
	WeaponSwap::Tracker tracker;
	Check(!tracker.Update(0, true, bow), "entering a game on set II is the baseline, not a swap");
	Check(!tracker.Update(100, true, bow), "an unchanged set says nothing");
	Check(!tracker.Update(200, std::nullopt, sword), "an unreadable set says nothing");
	tracker.Reset();
	Check(!tracker.Update(300, false, sword), "a new game takes a new baseline");
}

void LocalSwap() {
	WeaponSwap::Tracker tracker;
	tracker.Update(0, false, sword);
	Check(tracker.Update(100, true, bow) == 2, "a swap that moved the items is announced at once");
	Check(!tracker.Update(200, true, bow), "and only once");
	Check(tracker.Update(300, false, sword) == 1, "swapping back names set I");
}

void ServerSwap() {
	WeaponSwap::Tracker tracker;
	tracker.Update(0, false, sword);
	Check(!tracker.Update(100, true, sword), "a flag that flips before the items move waits for them");
	Check(tracker.Update(200, true, bow) == 2, "and speaks when they arrive");
	tracker.Update(300, true, bow);
	Check(!tracker.Update(400, false, bow), "items that moved just before the flag wait too");
	Check(!tracker.Update(400 + WeaponSwap::ItemMoveGraceMs - 1, false, bow), "until the grace ends");
	Check(tracker.Update(400 + WeaponSwap::ItemMoveGraceMs, false, bow) == 1, "then it names what is in hand");
}

void EmptySets() {
	WeaponSwap::Tracker tracker;
	tracker.Update(0, false, empty);
	Check(!tracker.Update(100, true, empty), "two empty sets have nothing to move");
	Check(tracker.Update(100 + WeaponSwap::ItemMoveGraceMs, true, empty) == 2, "so the swap is spoken after the grace");
}

void Phrases() {
	Strings::Select(Strings::Language::esES);
	Check(WeaponSwap::Phrase(2, {"Arco largo", "Carcaj"}) == "Armas 2: Arco largo, Carcaj", "the set and both hands are named");
	Check(WeaponSwap::Phrase(1, {"Mandoble"}) == "Armas 1: Mandoble", "a two-handed weapon fills the phrase alone");
	Check(WeaponSwap::Phrase(1, {}) == "Armas 1: sin armas", "an empty set says so");
	Check(WeaponSwap::Phrase(2, {"", "Escudo"}) == "Armas 2: Escudo", "an unnamed item is left out");
}
}

int main() {
	Baseline(); LocalSwap(); ServerSwap(); EmptySets(); Phrases();
	Passed();
}
