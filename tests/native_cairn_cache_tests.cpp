#include "cairn_order_cache.h"
#include "test_support.h"

#include <array>
#include <cstdint>

using D2RAccess::CairnStones::OrderCache;
using D2RAccess::CairnStones::Order;

int main() {
	OrderCache cache;
	const std::array<uint32_t, 5> liveOrder{2, 4, 0, 1, 3};
	Check(!cache.BeginCapture() && !cache.Read(10), "capture requires a ready game session");
	cache.Reset(10, false);
	Check(!cache.BeginCapture(), "joining alone does not admit scroll packets");
	cache.Reset(10, true);
	const auto firstEpoch = cache.BeginCapture();
	Check(firstEpoch.has_value(), "ready session admits scroll packets");
	Check(!cache.CompleteCapture(*firstEpoch, {2, 4, 0, 1, UINT32_MAX}), "raw indices are checked before narrowing");
	Check(!cache.CompleteCapture(*firstEpoch, {2, 4, 0, 1, 2}), "duplicate scroll symbols are refused");
	Check(!cache.Read(10), "invalid packets do not create an order");
	Check(cache.CompleteCapture(*firstEpoch, liveOrder), "valid scroll packet is captured");
	Check(!cache.CompleteCapture(*firstEpoch, liveOrder), "reopening the same scroll does not announce another order");
	Check(cache.Read(10) == Order{2, 4, 0, 1, 3}, "captured order has the packet's rank order");
	Check(!cache.Read(11), "other sessions cannot read the captured order");
	cache.Reset(0, false);
	Check(!cache.Read(10), "leaving clears the decoded order");
	cache.Reset(11, true);
	Check(!cache.CompleteCapture(*firstEpoch, liveOrder), "packet completion racing a reset is discarded");
	Check(!cache.Read(11), "new session begins without prior order");
	const auto secondEpoch = cache.BeginCapture();
	Check(secondEpoch && cache.CompleteCapture(*secondEpoch, liveOrder), "new session can capture its own packet");
	Check(!cache.Read(10) && cache.Read(11), "order is scoped to the new session");
	Passed();
}
