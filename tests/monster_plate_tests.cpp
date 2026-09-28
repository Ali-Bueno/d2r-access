#include "monster_plate.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace D2RAccess;

namespace {
// The game's own strings (local/lng/strings: strChampionFormatXp, Monster2Format,
// champfanaticX, Champion, Fallen).
void Forms() {
	using MonsterPlate::Form;
	Check(Form("Gefallener") == "Gefallener", "a phrase with no tags comes back whole");
	Check(Form("[ms]Gefallener") == "Gefallener", "a lone tag is not spoken");
	Check(Form("[ms]fanatischer[fs]fanatische[ns]fanatisches[pl]fanatische") == "fanatischer", "the first form by default");
	Check(Form("[ms]fanatischer[fs]fanatische[ns]fanatisches[pl]fanatische", "[fs]") == "fanatische", "the form for a tag");
	Check(Form("[ms]fanatischer[fs]fanatische", "[mp]") == "fanatischer", "a tag the phrase lacks takes the first form");
	Check(Form("").empty(), "an empty phrase stays empty");
}
void Names() {
	using MonsterPlate::Compose;
	std::vector<std::string> parts{"Champion", "Fallen"};
	Check(Compose("%0 %1", parts) == "Champion Fallen", "an English champion: its kind, then the base name");
	parts = {"[ms]fanatischer[fs]fanatische[ns]fanatisches[pl]fanatische", "Gefallener"};
	Check(Compose("a0n1:%0 %1", parts) == "fanatischer Gefallener", "the grammar header is not spoken; an untagged noun is masculine");
	parts = {"[ms]fanatischer[fs]fanatische[ns]fanatisches[pl]fanatische", "[fs]Hexe"};
	Check(Compose("a0n1:%0 %1", parts) == "fanatische Hexe", "the adjective agrees with the noun's tag");
	parts = {"[fs]campeona[ms]campeón[fp]campeonas[mp]campeones", "Caído"};
	Check(Compose("a0n1:%1 %0", parts) == "Caído campeón", "a format may put the noun first");
	parts = {"элита", "Падший"};
	Check(Compose("%1 (%0)", parts) == "Падший (элита)", "a format with no header fills its parts as they are");
	parts = {"Gloom", "Spawn", "the Hungry"};
	Check(Compose("%0 %1 %2", parts) == "Gloom Spawn the Hungry", "a random unique's three parts");
	parts = {"Gloom", "Spawn"};
	Check(Compose("%0 %1 %2", parts) == "Gloom Spawn", "a missing part leaves no trailing space");
	Check(Compose("100%", parts) == "100%", "a lone percent sign is kept");
}
}

int main() {
	Forms();
	Names();
	Passed();
}
