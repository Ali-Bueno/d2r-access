#pragma once
#include "vitals.h"
#include <D2RLPlugin/api.h>
#include <string>
#include <vector>

// What the local player is wearing, through the loader's inventory service.
namespace D2RAccess::EquipmentCensus {
enum class Result { Unavailable, NoPlayer, Read };
// Unavailable: the service or the enumeration failed, which must not read as
// "wearing nothing". NoPlayer: out of a game. UI thread only (inventory.h).
Result Read(const D2RL::PluginContext* context, std::vector<Vitals::Breakage::Item>& worn);
// The item's table name and quality, "" when its row cannot be read.
std::string Name(const Vitals::Breakage::Item& item);
}
