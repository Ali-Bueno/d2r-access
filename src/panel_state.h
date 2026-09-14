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

void Reset();

}
