#include "native_skill_icons.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"
#include "native_skill_bar.h"
#include "native_ui.h"
#include "native_widgets.h"

#include <string_view>

namespace D2RAccess::NativeSkillIcons {
namespace {
using NativeMemory::Peek;
// D2R 3.3.93787, READ in the image and not yet measured in the game; the
// working files are in the workspace, re/actions/skilltree and re/actions/widgets.
//
// SkillsTreePanel builds its skills in code when it opens (0x14C4520), in
// keyboard mode as in controller mode: Tab<n> > SkillButtonContainer >
// "SkillButton<page> <column>,<row>", a ButtonWidget whose click message
// carries the skill id. The message's payload pointer is at button +0x668 and
// the id is the i32 it points at, which is how the panel itself reads it
// (0x14C7D28 in 0x14C7720).
// The list of skills makes its icons as copies of ButtonTemplate under
// SelectionTray (0x14D9E90), each a SkillSelectButtonWidget with its skill id
// at +0xC08 (written by 0x239380).
constexpr size_t TreePayload = 0x668, ListSkill = 0xC08;
constexpr int MostSkills = 4096;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{0x14C4520, 6518, 0x7F4098ECCE760490ULL}, // the skill tree builds its skills
	{0x14C7720, 2895, 0xD4668882CEEB9F4DULL}, // and reads each one's id back
	{0x14D9E90, 4948, 0xE88F9BB7D8F282DFULL}, // the list makes its icons
	{0x239380, 1644, 0x0191E7C6E02DFE68ULL},  // an icon is given its skill
};
constexpr std::string_view TreeContainers[] = {"SkillButtonContainer", "CommonSkillsContainer", "ItemSkillsContainer"};
constexpr std::string_view ListContainer = "SelectionTray", ListIcon = "SkillSelectButtonWidget";
bool g_verified = false;
}

bool Initialize(const D2RL::PluginContext* context) {
	g_verified = context && LayoutCheck::Verify(context, context->exeBase, Checks, "skill-icons", "a skill is not named by its icon");
	if (g_verified) AsyncLog::Info(context, "[skill-icons] Verified.");
	return g_verified;
}

bool InTree(uintptr_t widget) {
	if (!widget || NativeUi::Classify(widget).kind != UiText::Kind::Button) return false;
	const uintptr_t container = NativeUi::Parent(widget);
	const auto name = NativeWidgets::ReadName(container);
	for (const auto known : TreeContainers)
		if (name == known) return NativeUi::Classify(NativeUi::Parent(container)).kind == UiText::Kind::TabButton;
	return false;
}

bool InList(uintptr_t widget) {
	return widget && NativeUi::Classify(widget).name == ListIcon && NativeWidgets::ReadName(NativeUi::Parent(widget)) == ListContainer;
}

std::optional<int> Skill(uintptr_t widget) {
	if (!g_verified) return std::nullopt;
	int32_t skill = -1;
	if (InList(widget)) {
		if (!Peek(widget + ListSkill, skill)) return std::nullopt;
	} else if (InTree(widget)) {
		uintptr_t payload = 0;
		if (!Peek(widget + TreePayload, payload) || !payload || !Peek(payload, skill)) return std::nullopt;
	} else return std::nullopt;
	if (skill < 0 || skill >= MostSkills) return std::nullopt;
	return skill;
}

std::string Name(uintptr_t widget) {
	const auto skill = Skill(widget);
	return skill ? NativeSkillBar::SkillName(*skill) : std::string{};
}
}
