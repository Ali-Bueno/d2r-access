#include "item_hand.h"

namespace D2RAccess::ItemHand {

Outcome Resolve(uint32_t heldBefore, uint32_t heldNow, const Under& before, const Under& after, bool filler, bool dropPressed) {
	if (!heldBefore || heldNow == heldBefore) return Outcome::None;
	// The game hands back the item it was put on when the two cannot combine.
	if (heldNow) return Outcome::Swapped;
	// Confirm socketing from the verified native filled count, not merely an
	// emptied hand over a socketed item. A stale or unknown count stays quiet.
	if (!dropPressed && before.id && before.id != heldBefore && after.id == before.id) {
		if (filler && before.sockets && after.sockets == before.sockets
			&& before.filled != UnknownFilled && after.filled != UnknownFilled
			&& before.filled < before.sockets && after.filled <= after.sockets
			&& after.filled == before.filled + 1) return Outcome::Socketed;
		if (after.quantity > before.quantity) return Outcome::Merged;
	}
	return dropPressed ? Outcome::Thrown : Outcome::Placed;
}

void Names::Remember(uint32_t id, const std::string& base, uint32_t quality, const std::string& name) {
	if (id && !name.empty()) names_[id] = {base, quality, name};
}

std::string Names::Recall(uint32_t id, const std::string& base, uint32_t quality) const {
	const auto found = names_.find(id);
	if (found == names_.end() || found->second.base != base || found->second.quality != quality) return {};
	return found->second.name;
}

}
