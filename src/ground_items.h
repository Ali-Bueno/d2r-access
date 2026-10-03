#pragma once

#include <cstdint>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace D2RAccess::GroundItems {
enum class Sound { Gold, Potion, Scroll, Rune, Gem, Normal, Magic, Rare, Legendary, Portal, Chest, Quest, Corpse,
	WeaponRack, Stash, WoodenDoor, MetalDoor, StoneDoor, SlimeDoor, UnexploredRoom, ZoneTransition, InsideUnexploredRoom, Discovered, Count };
// What an item is by its ItemTypes.txt ancestry; equipment is told apart by quality instead.
enum class Base { Equipment, Gold, Potion, Scroll, Rune, Gem };
enum class SourceKind { Item, Object };
struct Point { double x = 0, y = 0; };
struct Item {
	uint32_t id = 0, code = 0, quality = 0;
	Point position;
	Sound sound = Sound::Normal;
	std::string name, baseName;
	SourceKind source = SourceKind::Item;
};
// Landmarks, containers, quest objects and shut doors only loop; proximity speech is for items.
struct Snapshot { uint64_t session = 0; Point player; std::vector<Item> items, landmarks, containers, questObjects, doors; };
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
// Spacing for the rest of a pile; an item that has just come within reach speaks at once.
inline constexpr uint64_t SpeechGapMs = 900, PollMs = 50, SnapshotExpiryMs = 300;
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
inline constexpr SoundFile SoundFiles[] = {{"loot", "gold.wav"}, {"loot", "potion.wav"},
	{"loot", "scroll.wav"}, {"loot", "rune.wav"}, {"loot", "gem.wav"}, {"loot", "normal equipment.wav"},
	{"loot", "magic equipment.wav"}, {"loot", "rare equipment.wav"}, {"loot", "legendary equipment.wav"},
	{"exploration", "portal.wav"}, {"loot", "chest.wav", ChestLevelDb}, {"loot", "quest item.wav"}, {"loot", "corpse.wav"},
	{"loot", "weapon rack.wav"}, {"loot", "stash.wav", StashLevelDb},
	{"exploration", "wooden door.wav"}, {"exploration", "metal door.wav"}, {"exploration", "stone door.wav"},
	{"exploration", "slime door.wav"},
	{"exploration", "unexplored room.wav"}, {"exploration", "zone transition.wav"}, {"exploration", "inside unexplored room.wav"},
	{"exploration", "discovered.wav"}};
inline constexpr size_t ItemSoundCount = static_cast<size_t>(Sound::UnexploredRoom);
// The file list is indexed by Sound; a missing entry would be read past the end.
static_assert(std::size(SoundFiles) == static_cast<size_t>(Sound::Count));

Sound Classify(uint32_t quality, Base base, bool quest = false);
Voice Spatialize(const Item& item, Point player);
bool MatchesLabel(std::string_view label, const std::vector<std::string>& names);
class Tracker {
public:
	// Quest sources reserve a share first, then landmarks, doors, containers and ordinary items.
	// A quest item on the ground is an item; a quest object (a Cairn Stone) has its own switch.
	Update Tick(const Snapshot& snapshot, uint64_t now, bool speechEnabled, bool itemLoops = true,
		bool containerLoops = true, bool doorLoops = true, bool questLoops = true);
	void Reset();
private:
	uint64_t session_ = 0, lastSpeech_ = 0;
	bool hasSpoken_ = false;
	std::unordered_map<uint32_t, bool> spoken_;
	std::unordered_set<uint32_t> waiting_; // within reach but held back by the name cap or UI
};
}
