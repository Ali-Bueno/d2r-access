#pragma once
#include "native_ui.h"
namespace D2RL { struct PluginContext; }
namespace D2RAccess::BeltReader {
// BeltWidget's position routine divides the slot by four (0x22708A..0x22709A).
inline constexpr int Columns = 4, Rows = 4;
struct Readout {
	NativeUi::ItemSlot item;
	std::string location, summary;
};
std::string Location(int slot);
Readout Read(const D2RL::PluginContext* context, uintptr_t widget);
}
