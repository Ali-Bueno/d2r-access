#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

// What became of the item in hand, and the names the game's tooltips gave:
// the game shows no tooltip while an item is in hand. Pure state.
// docs/inventory-npc-followup.md#socketing
namespace D2RAccess::ItemHand {

// An item the focus rests on: its runtime id (0 for none), sockets and stack size.
inline constexpr uint32_t UnknownFilled = UINT32_MAX; // filled sockets that could not be read
struct Under { uint32_t id = 0, sockets = 0, filled = 0; int quantity = 0; };

enum class Outcome { None, Placed, Socketed, Merged, Swapped, Thrown };

// `heldBefore`/`heldNow`: runtime id in hand (0 when empty). `before`: what the
// focus rested on while the hand was full; `after`: what it rests on now.
// `filler`: the item in hand was a gem, rune or jewel. `dropPressed`: the drop
// button was what emptied the hand.
Outcome Resolve(uint32_t heldBefore, uint32_t heldNow, const Under& before, const Under& after, bool filler, bool dropPressed);

// Generated names by runtime id, trusted only while the item's table name and
// quality still match, so a recycled id from another game is not misnamed.
class Names {
public:
	void Remember(uint32_t id, const std::string& base, uint32_t quality, const std::string& name);
	std::string Recall(uint32_t id, const std::string& base, uint32_t quality) const;
	void Clear() { names_.clear(); }

private:
	struct Known { std::string base; uint32_t quality = 0; std::string name; };
	std::unordered_map<uint32_t, Known> names_;
};

}
