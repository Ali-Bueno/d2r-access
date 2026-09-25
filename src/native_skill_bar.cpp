#include "native_skill_bar.h"
#include "ground_item_layout.h"
#include "layout_check.h"
#include "localization.h"
#include "native_ground_items.h"
#include "native_memory.h"
#include "ui_text.h"
#include <windows.h>

namespace D2RAccess::NativeSkillBar {
namespace {
using NativeMemory::Peek;
// Getters and gates the HUD's SkillSelectButtonWidget uses; docs/skill-bar.md.
constexpr uintptr_t ControllerMode = 0x77E10, LtHeld = 0x14CF40, InputManager = 0x13CE90,
	BackBarGate = 0x13DE60, SkillAt = 0x117D20;
// SKILL_BUTTON enum (0x8BC500): SkillButton1..6, then AltSkillButton1..6.
constexpr int AlternateFirst = Buttons, NoSkill = -1;
// skills.bin and skilldesc.bin rows in the player's table bank (0x88850).
constexpr size_t Skills = 0x11B0, SkillCount = 0x11B8, SkillStride = 0x2EC, SkillDesc = 0x23C;
constexpr size_t Descs = 0x1190, DescCount = 0x1198, DescStride = 300, DescName = 8;
constexpr uint16_t MissingName = 0x1506; // 0x28BCF0's id for a skill without a description
constexpr GroundItemLayout::Evidence Checks[] = {
	{ControllerMode, 41, 0x290F3F607678EB67ULL},
	{LtHeld, 192, 0xF792A07C71DF5D43ULL},
	{InputManager, 123, 0x6E472424227624BDULL},
	{BackBarGate, 36, 0x4E8F9679DA45E341ULL},
	{SkillAt, 181, 0xDE7A76B2B41A63B9ULL},        // also pins the binding table 0x2A48A90
	{0x1195C0, 203, 0x7585B777DBDA2B53ULL},       // weapon-set offset the getter adds
	{0x88850, 308, 0x446510F5B6E36CE0ULL},        // skills -> skilldesc -> name id
	{0x28BCF0, 41, 0x499EC6729F9CD446ULL},        // name row offset and missing-name id
	{0x2399F0, 911, 0x3BE2F5C9B46E61ABULL},       // HUD button: LT bar index and condition
	{0x239380, 1644, 0x0191E7C6E02DFE68ULL},      // QuantityText shown only with a count
	{0x239110, 616, 0x983A761456B6E184ULL},       // controller actions and Town Portal count
};
uintptr_t g_base = 0;

bool ReadSkills(std::array<int, Buttons>& skills) noexcept {
	__try {
		if (!reinterpret_cast<bool(*)()>(g_base + ControllerMode)()) return false;
		const auto skillAt = reinterpret_cast<int(*)(int)>(g_base + SkillAt);
		for (int i = 0; i < Buttons; ++i) skills[i] = skillAt(AlternateFirst + i);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

std::string SkillName(uintptr_t tables, int skill) {
	uintptr_t skills = 0, descs = 0;
	uint32_t skillCount = 0, descCount = 0;
	int16_t desc = -1;
	uint16_t id = 0;
	if (skill < 0 || !Peek(tables + Skills, skills) || !Peek(tables + SkillCount, skillCount) || !skills
		|| static_cast<uint32_t>(skill) >= skillCount || !Peek(skills + skill * SkillStride + SkillDesc, desc)
		|| !Peek(tables + Descs, descs) || !Peek(tables + DescCount, descCount) || !descs
		|| desc < 0 || static_cast<uint32_t>(desc) >= descCount
		|| !Peek(descs + desc * DescStride + DescName, id) || id == 0 || id == MissingName) return {};
	return UiText::Clean(Localization::Text(uint32_t{id}));
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "skill-bar", "LT bar reading disabled")) return false;
	g_base = context->exeBase;
	return true;
}

std::optional<std::array<Binding, Buttons>> AlternateBar() {
	std::array<int, Buttons> skills{};
	uintptr_t player = 0, tables = 0;
	if (!g_base || !ReadSkills(skills) || !NativeGroundItems::RuntimeRoots(player, tables)) return std::nullopt;
	std::array<Binding, Buttons> bar;
	for (int i = 0; i < Buttons; ++i) {
		bar[i].skill = skills[i];
		bar[i].bound = skills[i] != NoSkill;
		if (bar[i].bound) bar[i].name = SkillName(tables, skills[i]);
	}
	return bar;
}

bool BackBarShown() noexcept {
	if (!g_base) return false;
	__try {
		return reinterpret_cast<bool(*)()>(g_base + LtHeld)()
			&& reinterpret_cast<bool(*)(uintptr_t)>(g_base + BackBarGate)(reinterpret_cast<uintptr_t(*)()>(g_base + InputManager)());
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
