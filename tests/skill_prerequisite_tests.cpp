#include "native_skill_bar.h"
#include "native_memory.h"
#include "native_ground_items.h"
#include "localization.h"
#include "test_support.h"
#include <array>
#include <cstring>

namespace D2RAccess::AsyncLog {
void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {}
void WarnF(const D2RL::PluginContext*, const char*, ...) noexcept {}
}
namespace D2RAccess::NativeGroundItems {
bool RuntimeRoots(uintptr_t&, uintptr_t&) noexcept { return false; }
}
namespace D2RAccess::Localization {
std::string Text(uint32_t id) {
	if (id == 100) return "Holy Fire";
	if (id == 101) return "Might";
	if (id == 102) return "Resist Fire";
	return {};
}
}
namespace {
using namespace D2RAccess;
template<size_t Size> struct Memory {
	std::array<uint8_t, Size> bytes{};
	uintptr_t Address() const { return reinterpret_cast<uintptr_t>(bytes.data()); }
	template<class T> void At(size_t offset, const T& value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); }
};
struct Fixture {
	Memory<0x11C0> bank;
	Memory<4 * 0x2EC> skills;
	Memory<4 * 300> descriptions;
	Fixture() {
		bank.At(0x11B0, skills.Address()); bank.At<uint32_t>(0x11B8, 4);
		bank.At(0x1190, descriptions.Address()); bank.At<uint32_t>(0x1198, 4);
		for (size_t id = 0; id < 4; ++id) {
			skills.At<int16_t>(id * 0x2EC + 0x23C, static_cast<int16_t>(id));
			skills.At(id * 0x2EC + 0x21E, std::array<int16_t, 3>{-1, -1, -1});
		}
		skills.At<uint8_t>(0x2EC + 0x2C, 3); // Paladin
		descriptions.At(300 + 2, std::array<uint8_t, 3>{1, 2, 2});
		descriptions.At<uint16_t>(300 + 8, 100);
		descriptions.At<uint16_t>(600 + 8, 101);
		descriptions.At<uint16_t>(900 + 8, 102);
		Required({2, -1, -1});
	}
	void Required(std::array<int16_t, 3> ids) { skills.At(0x2EC + 0x21E, ids); }
	NativeSkillBar::TreeSkill Read() { return NativeSkillBar::ReadClassSkill(bank.Address(), 3, 1, 2, 2); }
};
}
int main() {
	Fixture f;
	auto skill = f.Read();
	Check(skill.name == "Holy Fire" && skill.prerequisites == std::vector<std::string>{"Might"},
		"reqskill ids use the active table's localized names, independent of synergy skills");
	f.Required({2, 3, -1});
	Check(f.Read().prerequisites == std::vector<std::string>{"Might", "Resist Fire"},
		"a changed active table with multiple requirements is represented without a stock hardcoded graph");
	f.Required({2, 2, 3});
	Check(f.Read().prerequisites == std::vector<std::string>{"Might", "Resist Fire"}, "repeated ids do not duplicate requirements");
	f.Required({-1, -1, -1});
	Check(f.Read().prerequisites.empty() && f.Read().name == "Holy Fire", "a starting skill's no-requirement sentinels remain empty");
	f.Required({0, -1, -1}); f.descriptions.At<uint16_t>(8, 101);
	Check(f.Read().prerequisites == std::vector<std::string>{"Might"}, "skill zero is a valid id, not an empty requirement");
	for (const auto invalid : {std::array<int16_t, 3>{2, 99, -1}, {2, -2, -1}, {2, 1, -1}}) {
		f.Required(invalid);
		Check(f.Read().name == "Holy Fire" && f.Read().prerequisites.empty(), "bad or self-referencing ids do not expose partial requirements");
	}
	f.Required({2, 3, -1}); f.descriptions.At<uint16_t>(900 + 8, 999);
	Check(f.Read().prerequisites.empty(), "a missing localized dependency omits the incomplete list");
	f.descriptions.At<uint16_t>(900 + 8, 102);
	Check(NativeSkillBar::ReadClassSkill(f.bank.Address(), 2, 1, 2, 2).name.empty(), "another class's cell is not selected");
	Check(NativeSkillBar::ReadClassSkill(f.bank.Address(), 3, 257, 2, 2).name.empty(), "coordinates cannot wrap into a different cell");
	f.skills.At<uint8_t>(2 * 0x2EC + 0x2C, 3);
	f.descriptions.At(600 + 2, std::array<uint8_t, 3>{1, 2, 2});
	Check(f.Read().name.empty(), "ambiguous cells cannot borrow prerequisites from an arbitrary row");
	f.bank.At<uint32_t>(0x11B8, 0xFFFFFFFF);
	Check(f.Read().name.empty(), "corrupt counts are bounded before traversal");
	Check(NativeSkillBar::ReadClassSkill(0, 3, 1, 2, 2).name.empty(), "missing table bank returns no entry");
	Passed();
}
