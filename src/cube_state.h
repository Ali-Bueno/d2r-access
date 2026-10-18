#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace D2RL { struct PluginContext; }

// Where the Horadric Cube is and how it is named. docs/horadric-cube.md
namespace D2RAccess::CubeState {

// The cube's own window, which holding A on the carried cube opens.
inline constexpr std::string_view Panel = "HoradricCubeLayout";
// The cube the expansion stash's gem, rune and material tabs carry (BankExpansionLayoutHD.json).
inline constexpr std::string_view StashCube = "advancedstash_horadriccube", StashCubeGrid = "horadriccube_grid";

// Whether `widget` of the panel named `panel` is a cube's grid, in its window or in the stash.
inline bool IsGrid(std::string_view panel, std::string_view widget) {
	return widget == StashCubeGrid || (panel == Panel && widget == "grid");
}

inline constexpr std::string_view StashPanel = "BankExpansionLayout";

bool PanelOpen();
// The stash panel shows its cube beside the tab.
bool StashShown(uintptr_t stashPanel);
// Any cube is on screen: its window, or the stash's own.
bool Up();
// The cube is in the player's backpack (read at most twice a second).
bool Carried(const D2RL::PluginContext* context);
// The game's name for the item, English where it cannot be read.
std::string Name();
// `body` followed by " - Cube open" while a carried cube is shown in the stash; unchanged otherwise.
std::string WithStashStatus(const D2RL::PluginContext* context, const std::string& body, uintptr_t stashPanel);
// `help` with the game's own "Hold A to Open" when the focused item is the cube and the help lacks it.
std::string WithOpenHint(const D2RL::PluginContext* context, const std::string& help, uintptr_t item);

}
