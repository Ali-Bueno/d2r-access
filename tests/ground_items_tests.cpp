#include "ground_items.h"
#include "ground_item_text.h"
#include "speech_strings.h"
#include "test_support.h"
#include <D2RLPlugin/item.h>
#include <cmath>
#include <limits>

using namespace D2RAccess;
using namespace GroundItems;
namespace {
Item Make(uint32_t id, Point position = {}, std::string name = "Poción") {
	Item result; result.id = id; result.position = position; result.name = std::move(name); return result;
}
void Classification() {
	const Sound expected[] = {Sound::Normal, Sound::Normal, Sound::Normal, Sound::Normal,
		Sound::Magic, Sound::Legendary, Sound::Rare, Sound::Legendary, Sound::Rare};
	for (uint32_t q = 0; q < std::size(expected); ++q) {
		Check(Classify(q, false, false) == expected[q], "native equipment quality selects the appropriate loop");
		Check(Classify(q, true, false) == Sound::Gold, "gold overrides quality");
		Check(Classify(q, false, true) == Sound::Potion, "consumables override quality");
	}
	for (auto code : {D2RL::Items::MakeItemCode("bks"), D2RL::Items::MakeItemCode("tr1"), D2RL::Items::MakeItemCode("tr2"),
		D2RL::Items::MakeItemCode("xyz"), D2RL::Items::MakeItemCode("ice")})
		Check(Classify(2, false, false, code) == Sound::Potion, "quest scrolls and potions share the potion loop");
}
void Position() {
	const auto up = Spatialize(Make(1, {-5, -5}), {}), down = Spatialize(Make(1, {5, 5}), {});
	const auto left = Spatialize(Make(1, {-5, 5}), {}), right = Spatialize(Make(1, {5, -5}), {});
	Check(up.front > 0 && up.right == 0 && up.pitch == 1, "screen up is directly ahead at original pitch");
	Check(down.front < 0 && down.right == 0 && down.pitch == RearPitch, "screen down is behind at a gently lowered pitch");
	Check(left.right < 0 && std::abs(left.front) == 0 && left.pitch == 1, "screen left is on the left");
	Check(right.right > 0 && std::abs(right.front) == 0 && right.pitch == 1, "screen right is on the right");
	Check(up.gain == down.gain && left.gain == right.gain, "orientation does not change distance loudness");
	float previous = LoopVolume;
	for (int distance = 0; distance <= 40; ++distance) {
		const auto voice = Spatialize(Make(1, {static_cast<double>(distance), 0}), {});
		Check(std::isfinite(voice.gain) && voice.gain >= 0 && voice.gain <= previous, "attenuation decreases continuously with distance");
		previous = voice.gain;
	}
	Check(Spatialize(Make(1, {AudibleRadius, 0}), {}).gain == 0, "range boundary is silent");
	Check(Spatialize(Make(1, {0.1, 0.1}), {}).pitch > RearPitch, "rear transition is blended at the side axis");
	Check(Spatialize(Make(1, {NAN, 0}), {}).gain == 0, "nonfinite coordinates cannot reach the mixer");
}
void Tracking() {
	Tracker tracker;
	Snapshot scene{1, {}, {Make(1)}};
	Check(tracker.Tick(scene, 0, true).speech == "Poción", "first nearby item speaks immediately");
	for (uint64_t t : {50, 950, 10000}) Check(tracker.Tick(scene, t, true).speech.empty(), "stationary item stays silent regardless of time");
	scene.player = {SpeakRadius + 0.1, 0}; tracker.Tick(scene, 11000, true);
	scene.player = {}; Check(tracker.Tick(scene, 12000, true).speech.empty(), "small boundary jitter does not rearm speech");
	scene.player = {RearmRadius + 0.1, 0}; tracker.Tick(scene, 13000, true);
	scene.player = {}; Check(!tracker.Tick(scene, 14000, true).speech.empty(), "leaving and returning rearms the item");
	scene.items.clear(); Check(tracker.Tick(scene, 15000, true).voices.empty(), "pickup removes its sound immediately");
	scene.items = {Make(1)}; Check(!tracker.Tick(scene, 16000, true).speech.empty(), "a newly dropped item is announced again");
	scene.session++; Check(!tracker.Tick(scene, 17000, true).speech.empty(), "new game clears identities even if IDs are reused");
	scene.items.push_back(Make(2));
	Check(tracker.Tick(scene, 18000, true).speech == "Poción", "distinct item with the same name is still announced");
	scene.items.push_back(Make(3));
	Check(tracker.Tick(scene, 18100, true).speech.empty(), "speech delivery is rate limited");
	Check(!tracker.Tick(scene, 19000, true).speech.empty(), "pending nearby item is not discarded during the cooldown");
	tracker.Reset();
	Check(tracker.Tick(scene, 20000, false).speech.empty(), "UI can suppress proximity speech");
	Check(!tracker.Tick(scene, 21000, true).speech.empty(), "suppressed items remain eligible after closing a nonpausing menu");
	tracker.Reset(); scene.items = {Make(1, {}, "")};
	Check(tracker.Tick(scene, 22000, true).speech.empty(), "unavailable localized name does not consume an announcement");
	scene.items[0].name = "Oro"; Check(tracker.Tick(scene, 23000, true).speech == "Oro", "delayed localization is announced once available");
	tracker.Reset(); scene.items.clear();
	for (uint32_t i = 0; i < 20; ++i) scene.items.push_back(Make(i, {}, std::to_string(i)));
	auto batch = tracker.Tick(scene, 24000, true);
	Check(batch.voices.size() == MaxVoices && batch.speech == "0; 1; 2; 3", "piles keep a bounded mix and bounded speech groups");
	float gain = 0; for (const auto& voice : batch.voices) gain += voice.gain;
	Check(gain <= MixGainLimit + 0.0001f, "worst case pile has audio headroom");
	Check(tracker.Tick(scene, 25000, true).speech == "4; 5; 6; 7", "remaining objects in a pile are read in subsequent groups");
	tracker.Reset(); scene.items = {Make(1), Make(1)};
	batch = tracker.Tick(scene, 26000, true);
	Check(batch.voices.size() == 1 && batch.speech == "Poción", "duplicate traversal cannot produce duplicate voices or speech");
	scene.items = {Make(2, {NAN, 0})};
	batch = tracker.Tick(scene, 27000, true); Check(batch.voices.empty() && batch.speech.empty(), "corrupt positions stay silent");
	tracker.Reset(); scene.items = {Make(1)};
	Item exit = Make(50, {4, 4}, ""); exit.sound = Sound::Portal; scene.landmarks = {exit};
	batch = tracker.Tick(scene, 28000, true);
	Check(batch.voices.size() == 2 && batch.voices[0].sound == Sound::Portal && batch.speech == "Poción", "an exit loops with the portal sound and is never spoken");
	batch = tracker.Tick(scene, 29000, true, false);
	Check(batch.voices.size() == 1 && batch.voices[0].sound == Sound::Portal, "item loops off keeps the exit loop");
	scene.landmarks = {exit, exit};
	Check(tracker.Tick(scene, 30000, true).voices.size() == 2, "duplicate exits share one voice");
	scene.landmarks = {Make(50, {AudibleRadius, 0}, "")};
	Check(tracker.Tick(scene, 31000, true).voices.size() == 1, "an exit out of range is silent");
	scene.items.clear();
	for (uint32_t i = 0; i < 20; ++i) scene.items.push_back(Make(i, {}, std::to_string(i)));
	scene.landmarks = {exit};
	batch = tracker.Tick(scene, 32000, true);
	Check(batch.voices.size() == MaxVoices && batch.voices[0].sound == Sound::Portal, "a pile never starves the exit loop");
	scene.landmarks.clear();
	scene.items.clear();
}

void Containers() {
	Tracker tracker;
	Snapshot scene;
	scene.session = 9;
	const auto chest = [](uint32_t id, Point at = {2, 2}) {
		Item container = Make(id, at, "");
		container.sound = Sound::Chest;
		return container;
	};
	scene.containers = {chest(70)};
	auto batch = tracker.Tick(scene, 1000, true);
	Check(batch.voices.size() == 1 && batch.voices[0].sound == Sound::Chest, "a container nearby loops with the chest sound");
	Check(batch.speech.empty(), "and is never spoken, however close it is");
	scene.containers = {chest(70), chest(70)};
	Check(tracker.Tick(scene, 2000, true).voices.size() == 1, "the same container twice is still one voice");
	scene.containers = {chest(70, {AudibleRadius, 0})};
	Check(tracker.Tick(scene, 3000, true).voices.empty(), "a container out of range is silent");
	scene.containers = {chest(70)};
	Check(tracker.Tick(scene, 4000, true, true, false).voices.empty(), "container loops off silences them");
	Item exit = Make(50, {4, 4}, "");
	exit.sound = Sound::Portal;
	scene.landmarks = {exit};
	batch = tracker.Tick(scene, 5000, true, false, true);
	Check(batch.voices.size() == 2 && batch.voices[0].sound == Sound::Portal && batch.voices[1].sound == Sound::Chest,
		"exits are claimed before containers");
	// A crypt: more caskets than voices, with loot on the floor.
	scene.containers.clear();
	for (uint32_t i = 100; i < 120; ++i) scene.containers.push_back(chest(i, {2, 2}));
	scene.items.clear();
	for (uint32_t i = 0; i < 10; ++i) scene.items.push_back(Make(i, {}, std::to_string(i)));
	tracker.Reset();
	batch = tracker.Tick(scene, 6000, false);
	size_t containers = 0, items = 0;
	for (const auto& voice : batch.voices) {
		containers += voice.sound == Sound::Chest;
		items += voice.sound != Sound::Chest && voice.sound != Sound::Portal;
	}
	Check(batch.voices.size() == MaxVoices, "the mixer still fills its voices");
	Check(containers == MaxContainerVoices, "a room full of containers takes no more than its share");
	Check(items > 0, "so the loot on the floor is still audible");
	float total = 0;
	for (const auto& voice : batch.voices) total += voice.gain;
	Check(total <= MixGainLimit + 0.0001f, "and the gain limit covers exits, containers and items together");
}
}
int main() {
	Classification(); Position(); Tracking(); Containers();
	Strings::Select(Strings::Language::esES);
	Check(GroundItemText::BaseName("[fp]Saetas") == "Saetas", "live grammatical prefix is removed from bolts");
	Check(GroundItemText::BaseName("[fs]Hacha de mano") == "Hacha de mano", "item gender markers are not spoken");
	Check(GroundItemText::BaseName("[ms]ligero[fs]ligera") == "ligero", "grammatical variants are not concatenated");
	Check(GroundItemText::BaseName("Pergamino") == "Pergamino", "untagged item names are preserved");
	Check(GroundItemText::Describe("Hacha", 7, 0) == "Hacha, único", "Spanish equipment readout uses actual quality");
	Check(GroundItemText::Describe("Oro", 2, 37) == "37 Oro", "gold includes native amount");
	Check(GroundItemText::Describe("Poción", 2, 0) == "Poción", "ordinary consumables stay concise");
	Check(GroundItemText::Describe("", 4, 0).empty(), "missing names never become a quality-only announcement");
	Strings::Select(Strings::Language::enUS);
	Check(GroundItemText::Describe("Axe", 4, 0) == "Axe, magic", "English quality is available");
	Check(MatchesLabel("Hacha\nDaño: 1-5", {"Hacha"}), "nearby tooltip duplicates are recognized");
	Check(!MatchesLabel("Hachazo", {"Hacha"}) && !MatchesLabel("Charsi", {"Hacha"}), "different names and NPCs keep their speech");
	Passed();
}
