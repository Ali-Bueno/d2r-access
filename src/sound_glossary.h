#pragma once

#include "settings_list.h"

#include <vector>

// Every sound the mod plays, as entries at the end of the Ctrl+S list: landing
// on one names it and plays it, a change plays it again (a tester, 2026-10-11,
// after the Last Epoch mod's glossary). Each sound comes from its own player.
namespace D2RAccess::SoundGlossary {
std::vector<SettingsList::Entry> Entries();
// The game's own drop sounds (the Diablo II item set), each named after an item that makes it.
std::vector<SettingsList::Entry> DropEntries();
}
