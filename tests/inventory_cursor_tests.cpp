#include "inventory_cursor.h"
#include "test_support.h"
#include <D2RLPlugin/api.h>
#include <D2RLPlugin/inventory.h>

namespace {
using Result = D2RL::Inventory::Result;
bool available = true;
Result playerResult = Result::Success, cursorResult = Result::Success;
D2RL::PlayerHandle playerHandle = 1, queriedPlayer = 0;
D2RL::ItemHandle cursorHandle = D2RL::InvalidItemHandle;
Result Player(const D2RL::PluginContext*, D2RL::PlayerHandle* player) noexcept {
	*player = playerHandle;
	return playerResult;
}
Result Cursor(const D2RL::PluginContext*, D2RL::PlayerHandle player, D2RL::ItemHandle* item) noexcept {
	queriedPlayer = player;
	*item = cursorHandle;
	return cursorResult;
}
D2RL::InventoryServiceV1 inventory{
	.serviceSize = D2RL::InventoryServiceV1Size, .serviceVersion = D2RL::InventoryServiceV1Version,
	.getLocalPlayer = Player, .getCursorItem = Cursor,
};
D2RL::ServiceQueryResult Query(const D2RL::PluginContext*, D2RL::ServiceId id, uint32_t version, const void** service) noexcept {
	if (!available || id != D2RL::ServiceId::Inventory || version != D2RL::InventoryServiceV1Version)
		return D2RL::ServiceQueryResult::Unavailable;
	*service = &inventory;
	return D2RL::ServiceQueryResult::Success;
}
const D2RL::PluginApi api{.apiSize = D2RL::PluginApiSize, .queryService = Query};
const D2RL::PluginContext context{.contextSize = D2RL::PluginContextSize, .api = &api};
}

int main() {
	using D2RAccess::InventoryCursor::HoldingItem;
	Check(!HoldingItem(nullptr).has_value(), "missing context gives unknown cursor state");
	Check(HoldingItem(&context) == false, "successful empty cursor query is not holding an item");
	cursorHandle = 10;
	Check(HoldingItem(&context) == true && queriedPlayer == playerHandle, "held item query uses the captured local player");
	playerHandle = 2;
	Check(HoldingItem(&context) == true && queriedPlayer == 2, "each poll reacquires the player handle");
	cursorResult = Result::NotFound;
	Check(HoldingItem(&context) == false, "no cursor item is an empty cursor even if output remains stale");
	for (auto error : {Result::Busy, Result::Unavailable, Result::StaleHandle, Result::CallbackFault}) {
		cursorResult = error;
		Check(!HoldingItem(&context).has_value(), "failed cursor read is unknown rather than an item release");
	}
	cursorResult = Result::Success; playerResult = Result::Unavailable;
	Check(!HoldingItem(&context).has_value(), "missing local player is unknown");
	playerResult = Result::Success; playerHandle = D2RL::InvalidPlayerHandle;
	Check(!HoldingItem(&context).has_value(), "invalid player handle is not queried");
	playerHandle = 1; available = false;
	Check(!HoldingItem(&context).has_value(), "unavailable inventory service is unknown");
	available = true; inventory.serviceSize = 0;
	Check(!HoldingItem(&context).has_value(), "truncated service is rejected before reading callbacks");
	inventory.serviceSize = D2RL::InventoryServiceV1Size; inventory.getCursorItem = nullptr;
	Check(!HoldingItem(&context).has_value(), "missing cursor callback is rejected");
	inventory.getCursorItem = Cursor; inventory.getLocalPlayer = nullptr;
	Check(!HoldingItem(&context).has_value(), "missing player callback is rejected");
	Passed();
}
