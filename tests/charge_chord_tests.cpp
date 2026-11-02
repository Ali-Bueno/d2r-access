#include "charge_chord_synth.h"
#include "test_support.h"

#include <cstdlib>
#include <vector>

using namespace D2RAccess::ChargeChord;

namespace {
constexpr double Rate = 48000;
// The loudest sample of the next `ms` milliseconds.
int Peak(Synth& synth, int ms) {
	std::vector<int16_t> block(static_cast<size_t>(Rate * ms / 1000));
	synth.Render(block);
	int peak = 0;
	for (const auto sample : block) peak = std::max(peak, std::abs(static_cast<int>(sample)));
	return peak;
}
}

int main() {
	Synth synth(Rate);
	Check(synth.Silent() && Peak(synth, 50) == 0, "a new synth is silent");

	synth.Strike(1);
	const int one = Peak(synth, 100);
	Check(one > 0 && !synth.Silent(), "a strike sounds");
	Peak(synth, 3000);
	Check(synth.Silent(), "a struck chord fades out on its own");

	synth.Strike(3);
	const int three = Peak(synth, 100);
	Check(three > one, "three notes are louder than one");
	Check(three < 32767 / 2, "the full chord stays well under half of full scale");
	Peak(synth, 3000);

	synth.Hold(1);
	const int struck = Peak(synth, 100);
	Peak(synth, 3000);
	const int held = Peak(synth, 500);
	Check(held > 0 && !synth.Silent(), "a held note keeps sounding");
	Check(held < struck * 3 / 4, "a held note settles under its strike");
	synth.Hold(1);
	Check(Peak(synth, 500) <= held + 1, "holding a held note again does not strike it");
	synth.Hold(2);
	Check(Peak(synth, 100) > held, "the next charge adds its note");
	synth.Hold(0);
	Peak(synth, 3000);
	Check(synth.Silent(), "letting go fades the chord out");

	Synth full(Rate), more(Rate);
	full.Strike(NoteCount);
	more.Strike(9);
	Check(Peak(more, 100) == Peak(full, 100), "more charges than notes strike the full chord");
	Passed();
	return 0;
}
