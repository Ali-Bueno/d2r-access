#pragma once

#include "generated/drop_sounds.h"

#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace D2RAccess::GroundItems {
enum class Sound { Gold, Potion, Scroll, Rune, Gem, Normal, Magic, Rare, Legendary, Portal, Chest, Quest, Corpse,
	WeaponRack, Stash, WoodenDoor, MetalDoor, StoneDoor, SlimeDoor,
	// The Last Epoch mod's gold and potions, one glass sound per kind of potion (a tester's choice, behind a switch).
	EpochGold, EpochHealthPotion, EpochManaPotion, EpochRejuvenationPotion, EpochOtherPotion,
	Shrine, Well,
	UnexploredRoom, ZoneTransition, InsideUnexploredRoom, Discovered,
	// A doorway's creak (a tester's recording, 2026-10-20) and the sound of the units walked:
	// a beep, or a click (a tester, 2026-10-21: less beep, more click).
	Doorway, Step, StepClick,
	// The tutorial's message and step-done cues.
	TutorialMessage, TutorialStepDone,
	// A teleport pad's loop: bright where it leads somewhere new, muted where the player has stood.
	PadNew, PadBeen,
	// The game's own drop sounds (Generated::DropSoundFiles), an alternative to the
	// Diablo IV set behind a switch (a tester, 2026-10-13).
	DropFirst, Count = DropFirst + static_cast<int>(Generated::DropSoundFiles.size()) };
constexpr Sound DropSound(size_t index) { return static_cast<Sound>(static_cast<size_t>(Sound::DropFirst) + index); }
constexpr bool IsDrop(Sound sound) { return sound >= Sound::DropFirst && sound < Sound::Count; }
// What an item is by its ItemTypes.txt ancestry; equipment is told apart by quality instead.
// Potion is any other potion or elixir (stamina, antidote, thawing).
enum class Base { Equipment, Gold, Potion, HealthPotion, ManaPotion, RejuvenationPotion, Scroll, Rune, Gem };
// The item sounds to choose from (the user, 2026-10-13): the project's Diablo IV set, the
// same with the Last Epoch mod's gold and potions, or the sound each item makes as it lands,
// the default since 2026-11-02 (the user). The order is the saved setting's value.
enum class SoundSet { DiabloIV, LastEpoch, DiabloIIDrops, Count };
int ItemSoundSet();
void SetItemSoundSet(int set);
bool LastEpochSounds();
bool DropSounds();
// When more items lie near than there are voices, health and mana potions sound
// only in the voices the rest of the loot leaves (a tester, 2026-10-14).
bool PotionsLast();
void SetPotionsLast(bool on);
// An item's drop sound by its code; none for a code the item tables do not know.
std::optional<Sound> DropSoundOf(uint32_t code);
enum class SourceKind { Item, Object };
struct Point { double x = 0, y = 0; };
struct Item {
	uint32_t id = 0, code = 0, quality = 0;
	Point position;
	Sound sound = Sound::Normal; // what the item is: its reach, priority and speech order follow it
	std::optional<Sound> play;   // a sound played in its place (a drop sound)
	std::string name, baseName;
	SourceKind source = SourceKind::Item;
	bool common = false; // a health or mana potion: heard after the rest of the loot (PotionsLast)
	bool silver = false; // a white item the game draws gray (GroundItemText::IsSilver): kept by every loot filter level but Off
};
// The audible loot filter (a tester, 2026-10-14): with it on, only some drop loops
// sound, as a sighted player's loot filter hides items; LB in play turns it on and
// off. Every item still makes the game's own drop sound as it lands.
enum class EquipmentLoops { All, MagicUp, RareUp, SetUnique, Off, Count };
namespace LootFilter {
bool Active();
void SetActive(bool on);
bool Potions();
void SetPotions(bool on);
bool Gems();
void SetGems(bool on);
EquipmentLoops Equipment();
void SetEquipment(EquipmentLoops level);
// Whether an item's loop sounds under the filter as it stands. Gold, scrolls and
// quest items always do.
bool Loops(const Item& item);
}
// Landmarks, containers, quest objects, shut doors, shrines and wells only loop; proximity speech is for items.
struct Snapshot { uint64_t session = 0; Point player; std::vector<Item> items, landmarks, containers, questObjects, doors, shrines, doorways, pads; };
struct Voice {
	uint32_t id = 0; Sound sound{}; float right = 0, front = 0, gain = 0, pitch = 1;
	SourceKind source = SourceKind::Item;
};
struct Update { std::vector<Voice> voices; std::string speech; };

// Accessibility tuning in collision subtiles, not assumed native pickup ranges.
inline constexpr double AudibleRadius = 30, SpeakRadius = 3;
// User request: every pass over an item reads it again; one subtile of hysteresis stops edge repeats.
inline constexpr double RearmRadius = SpeakRadius + 1;
// User listening feedback: quest cues need a longer reach and gentler distance fade.
inline constexpr double QuestAudibleRadius = 45;
constexpr double AudibleRange(Sound sound) { return sound == Sound::Quest ? QuestAudibleRadius : AudibleRadius; }
inline constexpr size_t MaxVoices = 8, MaxNamesPerAnnouncement = 4;
// A crypt can hold a dozen caskets. Containers take at most this share of the
// voices so a room full of them cannot silence the loot on the floor.
inline constexpr size_t MaxContainerVoices = 3;
// A corridor of cells can line up a row of doors; the nearest ones are enough to steer by.
inline constexpr size_t MaxDoorVoices = 2;
// Reserve quest cues before ordinary sources without exhausting the exit/item mix.
inline constexpr size_t MaxQuestVoices = 2;
// A shrine and a well can stand together; the nearest two are what a passer-by needs.
inline constexpr size_t MaxShrineVoices = 2;
// Doorways between dungeon rooms, as many as doors.
inline constexpr size_t MaxDoorwayVoices = MaxDoorVoices;
// Teleport pads stand two of a kind in an Arcane Sanctuary room: a platform's pair is what is used from it.
inline constexpr size_t MaxPadVoices = 2;
// Spacing for the rest of a pile; an item that has just come within reach speaks at once.
inline constexpr uint64_t SpeechGapMs = 900, PollMs = 50, SnapshotExpiryMs = 300;
inline constexpr uint64_t LoaderConfigCheckMs = 2000; // the loader's options can change while the game runs
// Exits never move, so their list is refreshed slowly; only the listener position is polled fast.
inline constexpr uint64_t ExitRefreshMs = 500;
// Near-source gain for peak-normalized recordings; center panning halves each channel.
// User listening feedback: lower both individual sources and piles by about 3.5 dB.
inline constexpr float LoopVolume = 0.4f, RearPitch = 0.94f;
inline constexpr float MixGainLimit = 0.5f; // headroom even when nearby loops peak together
// Built from sounds/item loop sounds/<folder>/ and sounds/one shot sounds/<folder>/, and embedded
// in the DLL (cmake/GroundItems.cmake).
// `levelDb` trims one loop against the others after peak normalization.
struct SoundFile { const char* folder; const char* name; float levelDb = 0; };
// User listening, 2026-09-26: the chest sat about 4 LU under the loot loops, the stash too loud.
inline constexpr float ChestLevelDb = 4, StashLevelDb = -4;
// The walked distance click: a 30 ms burst carries 7 dB less than the 40 ms beep at one peak.
inline constexpr float StepClickLevelDb = 3;
inline constexpr SoundFile BaseSoundFiles[] = {{"loot", "gold.wav"}, {"loot", "potion.wav"},
	{"loot", "scroll.wav"}, {"loot", "rune.wav"}, {"loot", "gem.wav"}, {"loot", "normal equipment.wav"},
	{"loot", "magic equipment.wav"}, {"loot", "rare equipment.wav"}, {"loot", "legendary equipment.wav"},
	{"exploration", "portal.wav"}, {"loot", "chest.wav", ChestLevelDb}, {"loot", "quest item.wav"}, {"loot", "corpse.wav"},
	{"loot", "weapon rack.wav"}, {"loot", "stash.wav", StashLevelDb},
	{"exploration", "wooden door.wav"}, {"exploration", "metal door.wav"}, {"exploration", "stone door.wav"},
	{"exploration", "slime door.wav"},
	{"last epoch", "gold.wav"}, {"last epoch", "health potion.wav"}, {"last epoch", "mana potion.wav"},
	{"last epoch", "rejuvenation potion.wav"}, {"last epoch", "other potion.wav"},
	{"exploration", "shrine.wav"}, {"exploration", "well.wav"},
	{"exploration", "unexplored room.wav"}, {"exploration", "zone transition.wav"}, {"exploration", "inside unexplored room.wav"},
	{"exploration", "discovered.wav"}, {"exploration", "doorway.wav"}, {"movement", "step.wav"}, {"movement", "click.wav", StepClickLevelDb},
	{"tutorial", "message.wav"}, {"tutorial", "step done.wav"},
	{"exploration", "pad new.wav"}, {"exploration", "pad been.wav"}};
static_assert(std::size(BaseSoundFiles) == static_cast<size_t>(Sound::DropFirst));
inline constexpr std::string_view DropFolder = "d2 drops";
// Indexed by Sound: the files above, then the drop sounds.
inline constexpr auto SoundFiles = [] {
	std::array<SoundFile, static_cast<size_t>(Sound::Count)> files{};
	size_t at = 0;
	for (const auto& file : BaseSoundFiles) files[at++] = file;
	for (const char* name : Generated::DropSoundFiles) files[at++] = {DropFolder.data(), name};
	return files;
}();
inline constexpr size_t ItemSoundCount = static_cast<size_t>(Sound::UnexploredRoom);

// The gold carried, to say what a pile brought in once it is picked up (a tester:
// the amount on the ground was heard on every pass). Only a rise in play counts:
// a sale, a withdrawal or a reward happens in a menu.
class Purse {
public:
	// The gold picked up since the last sample, 0 for none; `gold` below 0 is unreadable.
	int Update(int gold, bool playing);
	void Reset() { known_ = -1; }
private:
	int known_ = -1;
};

Sound Classify(uint32_t quality, Base base, bool quest = false, bool lastEpoch = false);
// The sound an item makes in the chosen set: by its class, or with the Diablo II drops its own drop sound.
Sound Heard(uint32_t code, uint32_t quality, Base base);
bool IsPotion(Sound sound);
bool IsGold(Sound sound);
Voice Spatialize(const Item& item, Point player);
bool MatchesLabel(std::string_view label, const std::vector<std::string>& names);
class Tracker {
public:
	// Quest sources reserve a share first, then landmarks, doors, containers and ordinary items.
	// A quest item on the ground is an item; a quest object (a Cairn Stone) has its own switch.
	Update Tick(const Snapshot& snapshot, uint64_t now, bool speechEnabled, bool itemLoops = true,
		bool containerLoops = true, bool doorLoops = true, bool questLoops = true, bool shrineLoops = true);
	void Reset();
private:
	uint64_t session_ = 0, lastSpeech_ = 0;
	bool hasSpoken_ = false;
	std::unordered_map<uint32_t, bool> spoken_;
	std::unordered_set<uint32_t> waiting_; // within reach but held back by the name cap or UI
};
}
