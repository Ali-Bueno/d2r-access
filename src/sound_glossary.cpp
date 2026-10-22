#include "sound_glossary.h"
#include "beacon_audio.h"
#include "entity_cue_audio.h"
#include "ground_item_service.h"
#include "ground_items.h"
#include "item_audio.h"
#include "native_ground_items.h"
#include "radar_audio.h"
#include "speech_strings.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>

namespace D2RAccess::SoundGlossary {
namespace {
using Item = GroundItems::Sound;

// One radar reading straight up, as the radar's own sound test plays it.
std::function<void()> Radar(Radar::State state, bool unexplored = false, bool small = false) {
	return [=] {
		Radar::Events events{};
		Radar::Marks marks{}, smallMarks{};
		const auto up = static_cast<size_t>(Radar::Direction::Up);
		events[up] = state;
		marks[up] = unexplored;
		smallMarks[up] = small;
		RadarAudio::Play(events, {.eventMs = GetTickCount64(), .audition = true}, marks, smallMarks);
	};
}
std::function<void()> Beacon(BeaconAudio::Sound sound) { return [=] { BeaconAudio::Play(sound, {}); }; }
std::function<void()> Cue(EntityCueAudio::Sound sound) { return [=] { EntityCueAudio::Play(sound, {}); }; }
std::function<void()> Loot(Item sound) { return [=] { ItemAudio::Preview(sound); }; }

// In the order of Strings::Table::glossarySounds.
std::vector<std::function<void()>> Players() {
	using RS = Radar::State;
	using BS = BeaconAudio::Sound;
	using ES = EntityCueAudio::Sound;
	return {
		Radar(RS::Blocked), Radar(RS::Open), Radar(RS::Exit), Radar(RS::Exit, true), Radar(RS::Open, true), Radar(RS::Door),
		Radar(RS::Blocked, false, true), [] { RadarAudio::Bump(0, {.eventMs = GetTickCount64(), .audition = true}); },
		Beacon(BS::Locked), Beacon(BS::Ping), Beacon(BS::Arrived), Beacon(BS::Cleared),
		Cue(ES::Npc), Cue(ES::NpcAlert), Cue(ES::Boss), Cue(ES::Important), Cue(ES::Resurrector), Cue(ES::Ranged),
		Cue(ES::Ordinary), Cue(ES::Reach), Cue(ES::Sight), Cue(ES::Edge),
		Loot(Item::Gold), Loot(Item::Potion), Loot(Item::Scroll), Loot(Item::Rune), Loot(Item::Gem), Loot(Item::Normal),
		Loot(Item::Magic), Loot(Item::Rare), Loot(Item::Legendary), Loot(Item::Portal), Loot(Item::Chest), Loot(Item::Quest),
		Loot(Item::Corpse), Loot(Item::WeaponRack), Loot(Item::Stash), Loot(Item::WoodenDoor), Loot(Item::MetalDoor),
		Loot(Item::StoneDoor), Loot(Item::SlimeDoor), Loot(Item::UnexploredRoom), Loot(Item::ZoneTransition),
		Loot(Item::Discovered),
		Loot(Item::EpochGold), Loot(Item::EpochHealthPotion), Loot(Item::EpochManaPotion), Loot(Item::EpochRejuvenationPotion),
		Loot(Item::EpochOtherPotion), Loot(Item::Shrine), Loot(Item::Well), Cue(ES::Corpse),
		Loot(Item::Doorway), [] { ItemAudio::Preview(GroundItemService::StepBeep() ? Item::Step : Item::StepClick); },
	};
}
}

std::vector<SettingsList::Entry> Entries() {
	const auto players = Players();
	std::vector<SettingsList::Entry> entries;
	for (size_t i = 0; i < players.size() && i < Strings::GlossarySoundCount; ++i) {
		SettingsList::Entry entry;
		entry.on = [] { return false; };
		entry.set = [](bool) {};
		entry.say = [i](bool) {
			char phrase[200]{};
			std::snprintf(phrase, sizeof(phrase), Strings::Current().glossarySound, Strings::Current().glossarySounds[i]);
			return std::string(phrase);
		};
		entry.preview = players[i];
		entries.push_back(std::move(entry));
	}
	return entries;
}

std::vector<SettingsList::Entry> DropEntries() {
	std::vector<SettingsList::Entry> entries;
	for (size_t i = 0; i < Generated::DropSoundFiles.size(); ++i) {
		SettingsList::Entry entry;
		entry.on = [] { return false; };
		entry.set = [](bool) {};
		// The game's name of the example item, in the game's language once its tables are read.
		entry.say = [i](bool) {
			const auto classId = NativeGroundItems::ClassOfCode(Generated::DropSoundExamples[i]);
			std::string name = classId ? NativeGroundItems::ClassLabel(*classId, 0).baseName : std::string{};
			if (name.empty()) {
				name = Generated::DropSoundFiles[i];
				name = name.substr(0, name.rfind('.'));
				std::replace(name.begin(), name.end(), '_', ' ');
			}
			char phrase[200]{};
			std::snprintf(phrase, sizeof(phrase), Strings::Current().glossarySound, name.c_str());
			return std::string(phrase);
		};
		entry.preview = Loot(GroundItems::DropSound(i));
		entries.push_back(std::move(entry));
	}
	return entries;
}
}
