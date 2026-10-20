#include "test_support.h"
#include <array>
#include <cstring>
#include "../src/native_skill_hand.cpp"

// Exercise the production adapter with a mod-owned widget and callback. No
// game code, desktop input or running game is executed by these fixtures.
namespace {
int calls = 0;
uintptr_t received = 0;
void Click(uintptr_t widget) { ++calls; received = widget; }
}

int main() {
	using namespace D2RAccess::NativeSkillHand;
	std::array<uintptr_t, 25> table{};
	std::array<uint64_t, 380> widget{};
	const auto address = reinterpret_cast<uintptr_t>(widget.data());
	widget[0] = reinterpret_cast<uintptr_t>(table.data());
	table[ClickSlot / sizeof(uintptr_t)] = reinterpret_cast<uintptr_t>(&Click);
	auto side = [&](uint32_t value) { std::memcpy(reinterpret_cast<char*>(widget.data()) + SideOffset, &value, sizeof(value)); };
	g_base = 1; g_click = Click;
	side(1);
	Check(ActivateLeft(address) && calls == 1 && received == address, "visible left HUD target invokes its actual callback immediately");
	Check(ActivateLeft(address) && calls == 2, "repeat preserves native toggle semantics without deferred clicks");
	for (uint32_t other : {0u, 2u, 3u}) {
		side(other);
		Check(!ActivateLeft(address) && calls == 2, "list icon, right hand and binding button cannot be activated as left HUD");
	}
	side(1); table[ClickSlot / sizeof(uintptr_t)] = 1;
	Check(!ActivateLeft(address) && calls == 2, "unverified virtual click target fails closed");
	table[ClickSlot / sizeof(uintptr_t)] = reinterpret_cast<uintptr_t>(&Click);
	Check(!ActivateLeft(0) && !ActivateLeft(1) && calls == 2, "missing or unreadable widget never invokes callback");
	g_base = 0;
	Check(!ActivateLeft(address) && calls == 2, "unverified service cannot invoke native input");
	g_base = 1; g_click = nullptr;
	Check(!ActivateLeft(address) && calls == 2, "missing callback also fails closed");
	Passed();
}
