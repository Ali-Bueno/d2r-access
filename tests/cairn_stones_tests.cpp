#include "cairn_stones.h"
#include "test_support.h"
#include <algorithm>

using namespace D2RAccess::CairnStones;

int main() {
	Order order{0, 1, 2, 3, 4};
	do {
		Check(Valid(order), "every permutation of the five stones is a valid scroll order");
		States states;
		states.fill(State::Unactivated);
		for (size_t step = 0; step < Count; ++step) {
			Check(Rank(order, order[step]) == step + 1, "scroll rank maps to stone identity rather than distance");
			Check(Next(order, states) == order[step], "only the first unactivated stone in the scroll order is next");
			auto partial = states;
			partial[order[step]] = State::Unknown;
			Check(!Next(order, partial), "missing next stone does not select a later one");
			if (step > 0) {
				partial = states;
				partial[order[step - 1]] = State::Unknown;
				Check(!Next(order, partial), "unloaded previous stone cannot be assumed already activated");
			}
			if (step + 1 < Count) {
				partial = states;
				partial[order[step + 1]] = State::Activated;
				Check(!Next(order, partial), "out-of-order activation rejects a stale or inconsistent sequence");
			}
			states[order[step]] = State::Activated;
		}
		Check(!Next(order, states), "completed sequence has no next stone");
	} while (std::next_permutation(order.begin(), order.end()));
	Check(!Valid({0, 0, 2, 3, 4}) && !Valid({0, 1, 2, 3, 5}), "duplicate or out-of-range scroll symbols are rejected");
	Check(!Rank({0, 0, 2, 3, 4}, 0) && !Rank(order, Count), "invalid order cannot name a numbered stone");
	Check(!Next(order, States{}), "unknown world state is silent");
	Check(!Index(FirstClass - 1) && !Index(FirstClass + Count), "other objects cannot become Cairn Stones");
	for (size_t i = 0; i < Count; ++i) Check(Index(FirstClass + static_cast<uint32_t>(i)) == i, "native classes map to scroll symbols");
	Passed();
}
