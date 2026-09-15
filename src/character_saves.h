#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <string>
#include <vector>

// Offline characters, read from the game's own save files
// (<Saved Games>\Diablo II Resurrected\*.d2s): the character select screen
// draws its rows from these, but their text (name, class, level) is set at
// runtime and cannot be read through the SDK.
//
// Header layout measured on a D2R 3.3 save (file version 105, "yarkey":
// magic 0x55AA55AA at 0, version at 4, file size at 8, checksum at 0xC,
// last-played timestamp at 0x20, class at 0x18, level at 0x1B, name (16 bytes,
// NUL padded) at 0x12B). Compared with the documented version-99 layout the
// fields after the checksum sit 0x10 earlier and the name 0x20 later; only
// version 105 files are read, anything else is skipped rather than guessed.
namespace D2RAccess::CharacterSaves {

struct Character {
	std::string name;
	int         classIndex = -1; // 0 Amazon, 1 Sorceress, 2 Necromancer, 3 Paladin, 4 Barbarian, 5 Druid, 6 Assassin
	int         level      = 0;
	uint32_t    lastPlayed = 0; // Unix seconds
};

// Every readable save, most recently played first (the order the select
// screen lists them in). Re-reads the folder on each call: it is only asked
// when the player moves through the list. Logs what it found and why a file
// was skipped.
std::vector<Character> Load(const D2RL::PluginContext* context);

// String-table key of the class name for `classIndex`, or nullptr.
const char* ClassKey(int classIndex);

}
