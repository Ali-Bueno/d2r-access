#pragma once
#include "ground_items.h"
#include <D2RLPlugin/api.h>
#include <optional>
#include <string>

namespace D2RAccess::NativeGroundItems {
bool Initialize(const D2RL::PluginContext* context);
// Verified local player unit and its table bank; shared with NativeEntities.
bool RuntimeRoots(uintptr_t& player, uintptr_t& tables) noexcept;

// Gold the local player is carrying, or -1 when it cannot be read. The
// inventory's gold button carries no text of its own -- MEASURED 2026-09-20,
// [field] focus name="gold_button" ... text="" -- so the amount has to come
// from the player's stats, the same UNIT_GetStat call a gold pile on the
// ground already uses.
int PlayerGold() noexcept;
// Gold in the local player's personal stash, or -1 when it cannot be read.
int PlayerStashGold() noexcept;

struct UnitLabel { std::string baseName; uint32_t quality = 0; uint32_t id = 0; };
// Table name, quality and runtime id of a focused item; generated names come from its tooltip.
UnitLabel ItemUnitLabel(uintptr_t unit);
// The same for an item known by its class row (the SDK's ItemInfo::classId).
UnitLabel ClassLabel(uint32_t classId, uint32_t quality);
// Whether an item class goes into sockets: ItemTypes.txt makes gem, rune and jewel children of "sock".
bool SocketFiller(uint32_t classId);
// A weapon class: a bow or crossbow (ItemTypes.txt children of "miss"), and its melee rangeadder.
struct Weapon { bool missile = false; int rangeAdder = 0; };
std::optional<Weapon> WeaponOf(uint32_t classId);
bool Read(uint32_t playerId, uint64_t session, GroundItems::Snapshot& output);
// Guarded snapshot adapter, also exercised using constructed native memory.
bool ReadPlayer(uintptr_t player, uintptr_t tables, uint32_t playerId, uint64_t session, GroundItems::Snapshot& output);
}
