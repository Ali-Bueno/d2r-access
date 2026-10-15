#include "ground_items.h"
#include "ground_item_text.h"
#include "item_audio.h"
#include "speech_strings.h"
#include "test_support.h"

#include <algorithm>
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
		Check(Classify(q, Base::Equipment) == expected[q], "native equipment quality selects the appropriate loop");
		Check(Classify(q, Base::Gold) == Sound::Gold, "gold overrides quality");
		Check(Classify(q, Base::Potion) == Sound::Potion, "potions override quality");
		Check(Classify(q, Base::Scroll) == Sound::Scroll && Classify(q, Base::Rune) == Sound::Rune
			&& Classify(q, Base::Gem) == Sound::Gem, "scrolls, runes and gems have their own loops");
		Check(Classify(q, Base::Equipment, true) == Sound::Quest, "quest items override every native quality");
		Check(Classify(q, Base::Potion, true) == Sound::Quest, "quest consumables have their own cue");
	}
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
	Item chest = Make(2, {5, 0}), stash = Make(3, {5, 0});
	chest.sound = Sound::Chest; stash.sound = Sound::Stash;
	const float plain = Spatialize(Make(1, {5, 0}), {}).gain;
	Check(std::abs(Spatialize(chest, {}).gain / plain - std::pow(10.0f, ChestLevelDb / 20)) < 1e-4f
		&& std::abs(Spatialize(stash, {}).gain / plain - std::pow(10.0f, StashLevelDb / 20)) < 1e-4f,
		"the chest is raised and the stash lowered by their level trims");
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
	Check(tracker.Tick(scene, 18100, true).speech == "Poción", "an item just within reach does not wait for the previous group");
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
	Check(tracker.Tick(scene, 24050, true).speech.empty(), "the rest of a pile waits for the speech gap");
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

Item Make(uint32_t id, Sound sound, std::string name, Point position = {}) {
	Item result = Make(id, position, std::move(name)); result.sound = sound; return result;
}
// Health and mana potions wait for the voices the rest of the loot leaves (a tester, 2026-10-14).
void PotionsAfterLoot() {
	Tracker tracker;
	Snapshot scene{1, {}};
	for (uint32_t i = 0; i < MaxVoices; ++i) {
		auto potion = Make(i, Sound::Potion, "Poción", {1.0 + i, 0});
		potion.common = true;
		scene.items.push_back(potion);
	}
	scene.items.push_back(Make(100, Sound::Rare, "Anillo", {20, 0}));
	auto batch = tracker.Tick(scene, 1000, false);
	Check(batch.voices.size() == MaxVoices && std::any_of(batch.voices.begin(), batch.voices.end(), [](const Voice& voice) { return voice.id == 100; }),
		"a far ring is heard over the nearer potions");
	GroundItems::SetPotionsLast(false);
	batch = tracker.Tick(scene, 2000, false);
	Check(std::none_of(batch.voices.begin(), batch.voices.end(), [](const Voice& voice) { return voice.id == 100; }),
		"with the switch off the nearest are heard, as before");
	GroundItems::SetPotionsLast(true);
	scene.items.pop_back();
	Check(tracker.Tick(scene, 3000, false).voices.size() == MaxVoices, "with nothing else near, the potions take every voice");
}
// The audible loot filter keeps only some drop loops while it is on.
void LootFiltering() {
	using GroundItems::EquipmentLoops;
	namespace Filter = GroundItems::LootFilter;
	Tracker tracker;
	Snapshot scene{1, {}};
	scene.items.push_back(Make(1, Sound::Potion, "Poción", {2, 0}));
	scene.items.push_back(Make(2, Sound::Gem, "Rubí", {3, 0}));
	scene.items.push_back(Make(3, Sound::Normal, "Daga", {4, 0}));
	auto silver = Make(4, Sound::Normal, "Daga", {5, 0});
	silver.silver = true;
	scene.items.push_back(silver);
	scene.items.push_back(Make(5, Sound::Rare, "Anillo", {6, 0}));
	scene.items.push_back(Make(6, Sound::Gold, "Oro", {7, 0}));
	const auto heard = [&](uint64_t now) {
		std::vector<uint32_t> ids;
		for (const auto& voice : tracker.Tick(scene, now, false).voices) ids.push_back(voice.id);
		std::sort(ids.begin(), ids.end());
		return ids;
	};
	Check(heard(1000) == std::vector<uint32_t>{1, 2, 3, 4, 5, 6}, "with the filter off every drop loops");
	Filter::SetActive(true);
	Filter::SetEquipment(EquipmentLoops::RareUp);
	Check(heard(2000) == std::vector<uint32_t>{1, 2, 4, 5, 6},
		"rare and better keeps the rare, the silver white and the rest of the loot, not the plain white");
	Filter::SetPotions(false);
	Filter::SetGems(false);
	Filter::SetEquipment(EquipmentLoops::Off);
	Check(heard(3000) == std::vector<uint32_t>{6}, "with everything off only the gold loops");
	Filter::SetActive(false);
	Filter::SetPotions(true);
	Filter::SetGems(true);
	Filter::SetEquipment(EquipmentLoops::All);
	Check(heard(4000).size() == 6, "off again, every drop loops");
}

void SpeechPriority() {
	Tracker tracker;
	Snapshot scene{1, {}, {Make(1, Sound::Normal, "Gorro", {0.5, 0}), Make(2, Sound::Potion, "Poción", {1, 0}),
		Make(3, Sound::Gold, "37 Oro", {1.5, 0}), Make(4, Sound::Quest, "Cubo Horádrico", {2, 0})}};
	Check(tracker.Tick(scene, 1000, true).speech == "Cubo Horádrico; Poción; 37 Oro; Gorro",
		"one spot reads quest item, consumable and gold before the nearer equipment");
	scene.player = {RearmRadius + 2.1, 0}; tracker.Tick(scene, 1100, true);
	scene.player = {};
	Check(tracker.Tick(scene, 1200, true).speech == "Cubo Horádrico; Poción; 37 Oro; Gorro",
		"stepping off and straight back reads the pile again without waiting for the speech gap");

	tracker.Reset();
	scene.items = {Make(10, Sound::Magic, "Arco corto, mágico")};
	Check(tracker.Tick(scene, 4000, true).speech == "Arco corto, mágico", "equipment alone is read");
	scene.items.push_back(Make(11, Sound::Potion, "Poción"));
	Check(tracker.Tick(scene, 4050, true).speech == "Poción", "a potion arriving next is read at once");

	tracker.Reset(); scene.items.clear();
	for (uint32_t i = 0; i < 4; ++i) scene.items.push_back(Make(20 + i, Sound::Rare, "Equipo " + std::to_string(i)));
	scene.items.push_back(Make(30, Sound::Potion, "Poción"));
	scene.items.push_back(Make(31, Sound::Quest, "Pergamino de Inifuss"));
	Check(tracker.Tick(scene, 5000, true).speech == "Pergamino de Inifuss; Poción; Equipo 0; Equipo 1",
		"the name cap keeps quest items and consumables in the first group");
	Check(tracker.Tick(scene, 5000 + SpeechGapMs, true).speech == "Equipo 2; Equipo 3", "equipment beyond the cap follows");
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
void Doors() {
	Tracker tracker;
	Snapshot scene;
	scene.session = 10;
	const auto door = [](uint32_t id, Sound sound, Point at = {2, 2}) {
		Item shut = Make(id, at, "");
		shut.sound = sound;
		return shut;
	};
	// A cell block: a row of shut doors beside a chest and loot on the floor.
	for (uint32_t i = 0; i < 6; ++i) scene.doors.push_back(door(80 + i, i % 2 ? Sound::MetalDoor : Sound::WoodenDoor, {2.0 + i, 0}));
	Item chest = Make(70, {3, 3}, "");
	chest.sound = Sound::Chest;
	scene.containers = {chest};
	scene.items = {Make(1, {1, 1}, "Oro")};
	auto batch = tracker.Tick(scene, 1000, false);
	size_t doors = 0;
	for (const auto& voice : batch.voices) doors += voice.sound == Sound::WoodenDoor || voice.sound == Sound::MetalDoor;
	Check(doors == MaxDoorVoices, "a row of doors takes no more than its share");
	Check(batch.voices[0].id == 80 && batch.voices[1].id == 81, "and the nearest doors are the ones heard");
	Check(batch.voices.size() == MaxDoorVoices + 2, "leaving the chest and the loot audible");
	Check(tracker.Tick(scene, 2000, false, true, true, false).voices.size() == 2, "door loops off silences only the doors");
	scene.doors = {door(90, Sound::StoneDoor, {AudibleRadius, 0})};
	Check(tracker.Tick(scene, 3000, false, false, false).voices.empty(), "a door out of range is silent");
}
void QuestSources() {
	Tracker tracker;
	Snapshot scene{1};
	Item tree = Make(70, {10, -10}, "");
	tree.sound = Sound::Quest; tree.source = SourceKind::Object;
	Item scroll = Make(70, {-1, -1}, "Scroll of Inifuss");
	scroll.sound = Sound::Quest;
	scene.questObjects = {tree, tree};
	scene.items = {scroll, scroll};
	auto batch = tracker.Tick(scene, 1000, true);
	Check(batch.voices.size() == 2 && batch.speech == "Scroll of Inifuss",
		"quest item and object with equal numeric IDs remain distinct; duplicate reads do not add voices");
	Check(!ItemAudio::SameSource(batch.voices[0], batch.voices[1]), "playback keeps object and item voices independent");
	Check(ItemAudio::SameSource(batch.voices[0], Spatialize(scroll, {})), "stable quest identity preserves loop position");
	Check(batch.voices[1].right > 0 && batch.voices[0].front > 0, "quest sounds preserve screen direction");
	batch = tracker.Tick(scene, 2000, true, false);
	Check(batch.voices.size() == 1 && batch.voices[0].source == SourceKind::Object,
		"item sound toggle silences the quest pickup, not the quest object");
	batch = tracker.Tick(scene, 2100, true, true, true, true, false);
	Check(batch.voices.size() == 1 && batch.voices[0].source == SourceKind::Item,
		"quest object toggle silences the Cairn Stone, not the quest pickup");
	Check(tracker.Tick(scene, 2200, true, false, true, true, false).voices.empty(), "both toggles off silence every quest source");
	Check(tracker.Tick(scene, 3000, false, true, false).voices.size() == 2, "quest sources do not require container sounds or speech");
	scene.items.clear();
	Check(tracker.Tick(scene, 4000, false).voices.size() == 1, "picking up a quest item silences only that source");
	scene.questObjects.clear();
	Check(tracker.Tick(scene, 5000, false).voices.empty(), "using the quest object removes its source");
	scene.questObjects = {tree}; scene.questObjects[0].position = {AudibleRadius, 0};
	batch = tracker.Tick(scene, 6000, false);
	Check(batch.voices.size() == 1 && batch.voices[0].gain > LoopVolume / 4,
		"quest object is clearly audible at the old range boundary");
	scene.questObjects[0].position = {QuestAudibleRadius, 0};
	Check(tracker.Tick(scene, 6100, false).voices.empty(), "quest object falls silent at its extended range boundary");
	scroll.position = {QuestAudibleRadius - 1, 0}; scene.items = {scroll};
	batch = tracker.Tick(scene, 6200, true);
	Check(batch.voices.size() == 1 && batch.voices[0].gain > 0 && batch.speech.empty(),
		"distant ground quest item is audible without premature proximity speech");
	scene.items.clear();
	scroll.position = {-1, -1};
	Check(Spatialize(scene.questObjects[0], {}).gain == 0, "quest attenuation reaches zero at the new boundary");
	scene.questObjects[0].position = {NAN, 0};
	Check(tracker.Tick(scene, 7000, false).voices.empty(), "invalid quest coordinates stay silent");
	scene.questObjects = {tree}; scene.items = {scroll};
	for (uint32_t i = 0; i < 20; ++i) {
		Item exit = Make(i, {1, 0}, ""); exit.sound = Sound::Portal;
		scene.landmarks.push_back(exit);
		Item chest = Make(i, {1, 0}, ""); chest.sound = Sound::Chest;
		scene.containers.push_back(chest);
		scene.items.push_back(Make(i));
	}
	batch = tracker.Tick(scene, 8000, false);
	Check(batch.voices.size() == MaxVoices && batch.voices[0].sound == Sound::Quest && batch.voices[1].sound == Sound::Quest,
		"nearby exits, containers and ordinary loot cannot crowd out quest cues");
	Check(batch.voices[2].sound == Sound::Portal, "reserved quest share leaves room for exits");
	float total = 0; for (const auto& voice : batch.voices) total += voice.gain;
	Check(total <= MixGainLimit + 0.0001f, "quest sources share the pile gain limit");
}
}
// The game's own drop sounds play in place of an item's category, which keeps its reach.
void DropSoundSet() {
	using D2RL::Items::MakeItemCode;
	const auto gold = DropSoundOf(MakeItemCode("gld")), sword = DropSoundOf(MakeItemCode("ssd")), axe = DropSoundOf(MakeItemCode("hax"));
	Check(gold && sword && IsDrop(*gold) && IsDrop(*sword) && *gold != *sword, "gold and a sword have drop sounds of their own");
	Check(axe && SoundFiles[static_cast<size_t>(*axe)].folder == DropFolder, "a drop sound is one of the game's files");
	Check(!DropSoundOf(MakeItemCode("zzz")) && !DropSoundOf(0), "an unknown code has no drop sound");
	Check(DropSoundOf(MakeItemCode('g', 'l', 'd', '\0')) == gold, "a short code padded with NUL is found too");
	auto quest = Make(1, {AudibleRadius * 0.9, 0});
	quest.sound = Sound::Quest; quest.play = sword;
	const auto voice = Spatialize(quest, {});
	Check(voice.sound == *sword && voice.gain > 0, "the drop sound plays where the quest bell would, still at the quest's reach");
}

int main() {
	Classification(); Position(); Tracking(); SpeechPriority(); PotionsAfterLoot(); LootFiltering(); Containers(); Doors(); QuestSources(); DropSoundSet();
	Strings::Select(Strings::Language::esES);
	Check(GroundItemText::BaseName("[fp]Saetas") == "Saetas", "live grammatical prefix is removed from bolts");
	Check(GroundItemText::BaseName("[fs]Hacha de mano") == "Hacha de mano", "item gender markers are not spoken");
	Check(GroundItemText::BaseName("[ms]ligero[fs]ligera") == "ligero", "grammatical variants are not concatenated");
	Check(GroundItemText::BaseName("Pergamino") == "Pergamino", "untagged item names are preserved");
	// ItemNameMagicFormatSuffixOnly in the game's ui.json: esES takes the suffix for the noun.
	constexpr std::string_view SuffixOnlyEs = "a0n1:%1 %0", SuffixOnlyMx = "a1n0:%0 %1", SuffixOnlyEn = "%0 %1";
	Check(GroundItemText::RepairSuffixOnlyName("de llamas [fs]Maza", SuffixOnlyEs) == "Maza de llamas",
		"a suffix-only magic name is rebuilt in the order the format meant");
	Check(GroundItemText::RepairSuffixOnlyName("de la zorra [fs]Maza rebordeada[fp]Mazas rebordeadas", SuffixOnlyEs)
		== "Maza rebordeada de la zorra", "a multi-word base keeps one grammatical form");
	Check(GroundItemText::RepairSuffixOnlyName("Cimitarra carmesí de valor", SuffixOnlyEs) == "Cimitarra carmesí de valor",
		"a name the game resolved is left alone");
	Check(GroundItemText::RepairSuffixOnlyName("Coste: 433", SuffixOnlyEs) == "Coste: 433", "other lines are left alone");
	Check(GroundItemText::RepairSuffixOnlyName("[fs]Maza", SuffixOnlyEs) == "[fs]Maza", "a bare tagged base has no suffix to move");
	Check(GroundItemText::RepairSuffixOnlyName("de llamas [fs]Maza", SuffixOnlyMx) == "de llamas [fs]Maza",
		"a format that names the base as noun is trusted");
	Check(GroundItemText::RepairSuffixOnlyName("Mace of Flame", SuffixOnlyEn) == "Mace of Flame", "headerless formats are trusted");
	Check(GroundItemText::RepairSuffixOnlyName("de llamas [fs]Maza", "") == "de llamas [fs]Maza", "an unresolved format repairs nothing");
	Check(GroundItemText::Describe("Hacha", 7, 0) == "Hacha, único", "Spanish equipment readout uses actual quality");
	Check(GroundItemText::Describe("Oro", 2, 37) == "Oro", "a gold pile is named alone: its amount is said once picked up");
	{
		GroundItems::Purse purse;
		Check(purse.Update(100, true) == 0, "the first sample of the purse is a baseline");
		Check(purse.Update(137, true) == 37, "gold that comes in during play is a pickup");
		Check(purse.Update(500, false) == 0 && purse.Update(500, true) == 0, "gold that comes in from a menu is not");
		Check(purse.Update(400, true) == 0 && purse.Update(-1, true) == 0 && purse.Update(410, true) == 10,
			"spending and an unreadable sample say nothing, and the next pickup counts from what is left");
	}
	Check(GroundItemText::Describe("Poción", 2, 0) == "Poción", "ordinary consumables stay concise");
	Check(GroundItemText::Describe("", 4, 0).empty(), "missing names never become a quality-only announcement");
	Strings::Select(Strings::Language::enUS);
	Check(GroundItemText::Describe("Axe", 4, 0) == "Axe, magic", "English quality is available");
	{
		using namespace GroundItemText;
		Check(IsSilver(2, FlagSocketed) && IsSilver(2, FlagEthereal) && IsSilver(1, FlagSocketed) && IsSilver(3, FlagEthereal), "socketed or ethereal white qualities are gray");
		Check(!IsSilver(2, 0) && !IsSilver(2, FlagBroken), "a plain white item is not silver");
		for (uint32_t q = FirstColouredQuality; q <= LastColouredQuality; ++q)
			Check(!IsSilver(q, FlagSocketed | FlagEthereal), "magic and better keep their own colour");
		Check(!IsSilver(2, FlagSocketed | FlagBroken) && !IsSilver(2, FlagSocketed | FlagRuneword), "broken is red and a runeword is not silver");
		Check(Describe("Leather Armor", 2, 0, FlagEthereal) == "Leather Armor, silver", "silver is the colour word only");
		Check(Describe("Leather Armor", 4, 0, FlagEthereal) == "Leather Armor, magic", "a magic item is not silver");
		Check(Describe("Leather Armor", 3, 0, FlagSocketed) == "Leather Armor, superior, silver", "superior keeps its word");
		SetSilverEnabled(false);
		Check(Describe("Leather Armor", 2, 0, FlagEthereal) == "Leather Armor", "the switch silences it");
		SetSilverEnabled(true);
	}
	Check(MatchesLabel("Hacha\nDaño: 1-5", {"Hacha"}), "nearby tooltip duplicates are recognized");
	Check(!MatchesLabel("Hachazo", {"Hacha"}) && !MatchesLabel("Charsi", {"Hacha"}), "different names and NPCs keep their speech");
	Passed();
}
