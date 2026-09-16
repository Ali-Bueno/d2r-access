#pragma once

#include "native_ui.h"
#include <string>
#include <string_view>

namespace D2RAccess::ProgressionText {
// Runtime names are emitted as "SkillButton<tree> <column>,<row>" by D2R.
NativeUi::Cell SkillCell(std::string_view name);
// Skill tooltip lines are stored bottom to top; font-size commands are not text.
std::string SkillDescription(std::string_view body);
}
