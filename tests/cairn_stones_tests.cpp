#include "cairn_stones.h"
#include "test_support.h"
#include <algorithm>

using namespace D2RAccess::CairnStones;

int main() {
	Order order{0, 1, 2, 3, 4};
	do {
		Check(Valid(order), "every permutation of the five stones is a valid scroll order");
	} while (std::next_permutation(order.begin(), order.end()));
	Check(!Valid({0, 0, 2, 3, 4}) && !Valid({0, 1, 2, 3, 5}), "duplicate or out-of-range scroll symbols are rejected");
	Check(!Index(FirstClass - 1) && !Index(FirstClass + Count), "other objects cannot become Cairn Stones");
	for (size_t i = 0; i < Count; ++i) Check(Index(FirstClass + static_cast<uint32_t>(i)) == i, "native classes map to scroll symbols");
	Passed();
}
