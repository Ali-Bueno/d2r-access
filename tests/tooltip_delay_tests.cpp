#include "test_support.h"
#include "tooltip_delay.h"

#include <cstring>
#include <vector>

using namespace D2RAccess;

namespace {
std::vector<unsigned char> Panel(int32_t padding, int32_t delay) {
	std::vector<unsigned char> panel(0xB00);
	std::memcpy(panel.data() + 0xA40, &padding, sizeof(padding));
	std::memcpy(panel.data() + 0xA44, &delay, sizeof(delay));
	return panel;
}
int32_t Delay(const std::vector<unsigned char>& panel) {
	int32_t delay = 0;
	std::memcpy(&delay, panel.data() + 0xA44, sizeof(delay));
	return delay;
}
uintptr_t Address(std::vector<unsigned char>& panel) { return reinterpret_cast<uintptr_t>(panel.data()); }
}

int main() {
	auto controller = Panel(50, 400);
	TooltipDelay::Apply(nullptr, Address(controller));
	Check(Delay(controller) == 0, "the controller layout's 400 ms delay is cleared");
	TooltipDelay::Apply(nullptr, Address(controller));
	Check(Delay(controller) == 0, "and a cleared panel is left as it is");
	auto changed = Panel(50, 250);
	TooltipDelay::Apply(nullptr, Address(changed));
	Check(Delay(changed) == 250, "a delay the controller layout does not set is left alone");
	auto moved = Panel(0, 400);
	TooltipDelay::Apply(nullptr, Address(moved));
	Check(Delay(moved) == 400, "so is a panel whose padding says the fields are not where they were");
	TooltipDelay::Apply(nullptr, 0);
	Passed();
}
