#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The mercenary panel (HirelingInventoryPanel): the mercenary's name, level
// and type, experience, life, strength, dexterity, damage, defense and
// resistances, and up to three skill icons. In the controller layout nothing
// but the four equipment slots is focusable, so none of it can be reached by
// moving the focus; the Advanced Stats panel's stick reader reads it out when
// that panel is opened from here (AdvancedStatsReader).
//
// Nothing here is spent or raised: the panel has no point buttons and no
// points-remaining text (READ, HirelingInventoryPanelHD.json and its
// controller variant), and every "Stat Points" / "Skill Points" string in the
// game's tables belongs to the character.
namespace D2RAccess::MercenaryStats {

inline constexpr std::string_view Panel = "HirelingInventoryPanel";

// The mercenary's name, empty until the game has rendered it.
std::string Name(uintptr_t panel);

// Its level and type as the panel states them (HireTypeText).
std::string LevelAndType(uintptr_t panel);

// Its experience as "current of next level", from CurrentXP and NextLevelXP;
// the controller layout has only a bar, so then the percent within its level
// from the source below; empty when neither is known.
std::string Experience(uintptr_t panel);
// The mercenary's experience percent within its level, -1 unknown.
void SetExperienceSource(int (*source)());

// Every statistic row, label then value, in the order the layout declares
// them, which is the order down the screen. Each value is looked up among its
// own row's siblings, as the character panel's are. A row with no label is
// skipped, as are rows the game has hidden.
std::vector<std::string> Lines(uintptr_t panel);

struct Skill {
	int id = -1;
	int level = -1;
	// The skill's description as the game built it for the icon's tooltip:
	// the engine's bottom-up tooltip text, for TooltipText::Read. Empty when
	// it could not be read.
	std::string text;
};

// The skills the panel's icons show, in icon order (Skill0 to Skill2); icons
// the game has hidden -- a mercenary with fewer skills -- are left out.
std::vector<Skill> Skills(uintptr_t panel);
// One line per skill, its name first and then its description, for the
// panel's stick reader: the icons beside the experience bar are focusable
// only by the mouse (a tester could not hear them).
std::vector<std::string> SkillLines(uintptr_t panel);
// The skill one icon shows (a HirelingSkillIconWidget); its text is empty
// when it could not be read.
Skill SkillOf(uintptr_t icon);

// "120 / 150", which is how the panel states life, as "120 of 150" -- the
// words the character panel's life already uses. Anything else is returned
// unchanged.
std::string CurrentOfMaximum(std::string_view text);

}
