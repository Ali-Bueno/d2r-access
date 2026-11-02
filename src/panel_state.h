#pragma once

#include <string>
#include <string_view>
#include <vector>

// Centralized record of which game panels are open, in opening order. The
// announcer asks it what changed instead of echoing raw events: the game opens
// the next panel before closing the previous one, so a close of a panel that is
// no longer on top is a background transition the player does not need to hear.
namespace D2RAccess::PanelState {

// Records the panel as open (moving it to the top if already open).
void Opened(std::string_view panel);

// Records the panel as closed. Returns true when it was the active (top) panel.
bool Closed(std::string_view panel);

// Name of the active panel, or "" when none is open.
std::string_view Active();

// Every open panel, oldest first (the active one is last).
const std::vector<std::string>& OpenPanels();

// A panel of the in-game HUD, open all game and never a menu, by its layout name. The game
// gives the paladin's aura panel focus while loading into a game (a tester, 2026-10-22).
bool Hud(std::string_view panel);

// The title the window announcer has just spoken for a panel it opened. The
// controller's menu switcher then reports the same panel as a tab change, and
// saying that name a second time -- with interrupt -- cut off whatever had
// been queued behind the first.
void Announced(std::string_view title);

// True, once, when a tab change names exactly the title just announced.
bool TakeAnnounced(std::string_view tab);

// An act cinematic covers the game without any panel the record would show.
// Started by "MoviePlayerMessage:Play", ended by "Cinematics:MovieComplete" (or 8 minutes on, should
// that message never come). While one plays the cues pause and the mod's keys are the game's.
void MovieStarted();
void MovieEnded();
bool MoviePlaying();

void Reset();

}
