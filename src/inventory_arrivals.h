#pragma once

#include <cstdint>
#include <vector>

// Items that came into the backpack or the belt without passing through the
// hand: bought, taken off, handed over. Nothing on screen says where they went.
// Pure state. docs/inventory-npc-followup.md#arrivals
namespace D2RAccess::InventoryArrivals {

enum class Place { Other, Backpack, Belt, Worn };

// One of the player's items: runtime id, what it is and where it lies (top-left cell).
struct Item { uint32_t id = 0, classId = 0, quality = 0; Place place = Place::Other; int x = 0, y = 0; };

struct Arrival { Item item; bool unequipped = false; };

// What `now` holds in the backpack or the belt that `before` did not, except
// the item that was in hand (`heldBefore`): its placing is already spoken.
std::vector<Arrival> Diff(const std::vector<Item>& before, const std::vector<Item>& now, uint32_t heldBefore);

}
