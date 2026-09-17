#pragma once

#include "native_ui.h"
#include <string>
#include <string_view>

namespace D2RAccess::ProgressionText {
// Runtime names are emitted as "SkillButton<tree> <column>,<row>" by D2R.
NativeUi::Cell SkillCell(std::string_view name);
// Skill tooltip lines are stored bottom to top; font-size commands are not text.
std::string SkillDescription(std::string_view body);
// Put actionable state immediately after the skill name, before long bonuses.
// Grid position is not part of it: it travels in Readout::coordinates and is
// spoken last.
std::string SkillReadout(std::string_view description, std::string_view points, std::string_view help);
}
