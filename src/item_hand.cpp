#include "item_hand.h"

namespace D2RAccess::ItemHand {

Outcome Resolve(uint32_t heldBefore, uint32_t heldNow, const Under& before, const Under& after, bool filler, bool dropPressed) {
	if (!heldBefore || heldNow == heldBefore) return Outcome::None;
	// The game hands back the item it was put on when the two cannot combine.
	if (heldNow) return Outcome::Swapped;
	// Another item that stayed under the focus took the carried one in: a gem
	// into its socket, or a stack that grew. The SDK's filled-socket count is not
	// trusted: it read 0 for a filled socket (live, 2026-09-28).
	if (!dropPressed && before.id && before.id != heldBefore && after.id == before.id) {
		if (filler && before.sockets) return Outcome::Socketed;
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
