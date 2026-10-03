#pragma once
#include "native_ui.h"
#include <string>
namespace D2RL { struct PluginContext; }
// What a belt cell says: its D-pad button, its row and how many items its column holds. docs/belt.md
namespace D2RAccess::BeltReader {
// BeltWidget's position routine divides the slot by four (0x22708A..0x22709A).
inline constexpr int Columns = 4, Rows = NativeUi::BeltSlots / Columns;
struct Readout {
	NativeUi::BeltSlot item;
	uintptr_t automatic = 0; // the inventory potion drawn in an empty bottom cell
	std::string location, summary;
};
std::string Location(int slot);
Readout Read(const D2RL::PluginContext* context, uintptr_t widget);
}
