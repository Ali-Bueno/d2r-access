#include "monster_names.h"
#include "test_support.h"

using namespace D2RAccess::MonsterNames;
namespace {
constexpr std::string_view Carver = "Trinchante. Demonio", Shaman = "Chamán trinchante. Demonio";

void Reach() {
	Gate gate;
	Check(!gate.Admit(Carver, 12.0, false, 3.0, 0), "a monster beyond attack reach is not named");
	Check(gate.Admit(Carver, 2.5, false, 3.0, 100), "it is named once it comes within reach");
	Check(gate.Admit(Carver, 2.5, false, 3.0, 200), "and stays admitted while the plate keeps showing it");
	Gate elite;
	Check(elite.Admit("Rakanishu", 12.0, true, 3.0, 0), "bosses and elites are named from first sight");
	Gate unknown;
	Check(unknown.Admit(Carver, std::nullopt, false, 3.0, 0), "a label no unit carries keeps today's behaviour");
	Gate noReach;
	Check(noReach.Admit(Carver, 12.0, false, std::nullopt, 0), "an unknown reach drops only the reach rule");
}

// The pattern of the 2026-09-29 log: the target traded every 160 ms to 1.1 s.
void Trading() {
	Gate gate;
	uint64_t now = 0;
	Check(gate.Admit(Carver, 2.0, false, 15.0, now), "the first name is spoken");
	now += 160;
	Check(gate.Admit(Shaman, 2.0, false, 15.0, now), "a different name is spoken");
	for (int round = 0; round < 20; ++round) {
		now += 160;
		Check(!gate.Admit(round % 2 ? Shaman : Carver, 2.0, false, 15.0, now), "names traded back and forth stay silent");
	}
	now += 9000; gate.Absent();
	Check(!gate.Admit(Carver, 2.0, false, 15.0, now), "still silent within the repeat window of its last showing");
	gate.Absent();
	now += 10001;
	Check(gate.Admit(Carver, 2.0, false, 15.0, now), "a name long gone is spoken again");
}

void Interruptions() {
	Gate gate;
	Check(gate.Admit(Carver, 2.0, false, 3.0, 0), "named");
	gate.Absent();
	Check(!gate.Admit(Carver, 2.0, false, 3.0, 100), "a plate that flickers off and on does not repeat the name");
	Gate edge;
	Check(edge.Admit(Carver, 2.9, false, 3.0, 0), "named at the edge");
	Check(!edge.Admit(Carver, 3.1, false, 3.0, 100), "stepping out of reach silences it");
	Check(!edge.Admit(Carver, 2.9, false, 3.0, 200), "and stepping back in does not repeat it");
	edge.Reset();
	Check(edge.Admit(Carver, 2.9, false, 3.0, 300), "a reset forgets every name");
	Gate clock;
	Check(clock.Admit(Carver, 2.0, false, 3.0, 5000), "named");
	clock.Absent();
	Check(clock.Admit(Carver, 2.0, false, 3.0, 10), "a backwards clock forgets instead of silencing forever");
}

void Speech() {
	Check(Speakable("Chamán trinchante. Demonio \xE2\x80\xA2 Invoca trinchantes")
		== "Chamán trinchante. Demonio, Invoca trinchantes", "the affix dot is a pause");
	Check(Speakable("Ruptura. Demonio \xE2\x80\xA2 Extra fuerte \xE2\x80\xA2 Encantamiento de aura")
		== "Ruptura. Demonio, Extra fuerte, Encantamiento de aura", "every affix dot is a pause");
	Check(Speakable("\xE2\x80\xA2 Rápido") == "Rápido" && Speakable("Zombi \xE2\x80\xA2") == "Zombi",
		"leading and trailing dots vanish");
	Check(Speakable("Zombi. Muerto viviente") == "Zombi. Muerto viviente", "a plain label is unchanged");
}
}

int main() {
	Reach(); Trading(); Interruptions(); Speech();
	Passed();
}
