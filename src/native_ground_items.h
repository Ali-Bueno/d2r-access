#pragma once
#include "ground_items.h"
#include <optional>
#include <D2RLPlugin/api.h>
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

// An item unit's base name from the game's item table, as a ground item is
// named -- enough for a belt's potions and scrolls, which have no quality
// prefix. Empty when the unit is not an item or cannot be read.
std::string ItemBaseName(uintptr_t unit);
// The four-byte code in an item unit's items table row -- the one the ground
// item reader classifies by -- or 0 when the unit is not an item or cannot be
// read. What src/generated/item_sizes.h is keyed by.
uint32_t ItemTableCode(uintptr_t unit);
// The same code for an item class id, as the loader's item records give it; 0 when unreadable.
uint32_t ClassCode(uint32_t classId);
// The inverse: the class id of an item table code, found by scanning the table once.
std::optional<uint32_t> ClassOfCode(uint32_t code);
struct UnitLabel { std::string baseName; uint32_t quality = 0; uint32_t id = 0; uint32_t flags = 0; };
// Table name of an item known by its class row (the SDK's ItemInfo::classId),
// with the quality it was given.
UnitLabel ClassLabel(uint32_t classId, uint32_t quality);
bool SocketFiller(uint32_t classId);
// Whether an item class descends from an ItemTypes.txt code ("gem ", "rune"), parents included.
bool ClassIsType(uint32_t classId, uint32_t typeCode);
struct Weapon { bool missile = false; int rangeAdder = 0; };
std::optional<Weapon> WeaponOf(uint32_t classId);
// The same for an item unit, with the quality its unit data holds.
UnitLabel ItemUnitLabel(uintptr_t unit);
bool Read(uint32_t playerId, uint64_t session, GroundItems::Snapshot& output);
// The scanner's read: every item on the ground in the loaded rooms of the
// player's level, however far, where Read keeps to the player's room, its
// neighbours and the item sounds' reach (the user, 2026-09-28: items in other
// rooms dropped out of the scanner).
bool ReadLevel(uint32_t playerId, uint64_t session, GroundItems::Snapshot& output);
// Guarded snapshot adapter, also exercised using constructed native memory.
bool ReadPlayer(uintptr_t player, uintptr_t tables, uint32_t playerId, uint64_t session, GroundItems::Snapshot& output);
}
