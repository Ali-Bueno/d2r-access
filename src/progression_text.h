#pragma once

#include "native_ui.h"
#include "tooltip_text.h"
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::ProgressionText {
// Runtime names are emitted as "SkillButton<tree> <column>,<row>" by D2R.
struct SkillButton { int tree = -1; NativeUi::Cell cell; };
SkillButton ParseSkillButton(std::string_view name);
NativeUi::Cell SkillCell(std::string_view name);
// Skill tooltip lines are stored bottom to top; font-size commands are not text.
std::string SkillDescription(std::string_view body);
// The same tooltip as its lines, name first, for reading one line at a time.
std::vector<std::string> SkillLines(std::string_view body);
// The skill's name and the state the player can act on -- its points and its
// binding -- as the headline, and the explanation of what it does as the
// detail, which is read on demand rather than every time the highlight moves.
// Grid position is in neither: it travels in Readout::coordinates and is
// spoken last.
TooltipText::Description SkillReadout(std::string_view description, std::string_view points, std::string_view help);
}
