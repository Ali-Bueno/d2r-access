#include "hire_stats.h"
#include "auto_detail.h"
#include "character_stats_reader.h"
#include "input_direction.h"
#include "keyboard_menu.h"
#include "localization.h"
#include "mercenary_stats.h"
#include "native_skill_bar.h"
#include "native_tooltips.h"
#include "native_widgets.h"
#include "pad_filter.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "test_support.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace D2RAccess;

namespace {
uintptr_t trackedPanel = 0;
NativeTooltips::Snapshot tooltip;
InputDirection::StickPush pending = InputDirection::StickPush::None;     // the right stick
InputDirection::StickPush pendingLeft = InputDirection::StickPush::None; // the left stick, under Y
bool holdingY = false;
bool keyboard = false;   // the keyboard's menu cursor is up
bool lineStep = false;   // the right-stick push is a keyboard line step
bool autoDetail = false; // details are read unasked
// Widgets of the third row: 50 and 51 its skill icons, 60 its price's tooltip
// inside the container 61; 70 the panel's own gold line.
constexpr uintptr_t Jab = 50, Aura = 51, Price = 60, PriceBox = 61, Gold = 70;
std::vector<std::string> speech;
const D2RL::PluginContext context{};
bool Tick(NativeUi::Focus focus) { return HireStats::Tick(&context, 1000, focus); }
// A push on the left stick, the reader's while Y is held.
bool Push(NativeUi::Focus focus, InputDirection::StickPush push) { pendingLeft = push; speech.clear(); return Tick(focus); }
}

namespace D2RAccess::NativeWidgets {
std::string ReadName(uintptr_t id) {
	return id == 10 ? "HireMenuPanel" : id == 30 ? "TooltipsPanel" : id == Price ? "GoldCostTooltipWidget"
		: id == Gold ? "YourGoldTooltipWidget" : "OtherPanel";
}
uintptr_t FindPanel(std::string_view name) { return name == "TooltipsPanel" ? 30 : 0; }
uintptr_t FindChild(uintptr_t parent, std::string_view name) {
	return parent == 13 && name == "HireName" ? 40 : parent == PriceBox && name == "HireCost" ? 41
		: parent == 10 && name == "GoldText" ? 42 : 0;
}
std::string ReadText(uintptr_t id) { return id == 40 ? "Aliza" : id == 41 ? "717" : id == 42 ? "12,345" : ""; }
}
namespace D2RAccess::Localization {
std::string Text(std::string_view key) {
	return key == "HireMenuPanelYourGoldIconTooltip" ? "Your current gold" : key == "HireMenuPanelMercenaryGoldCostTooltip" ? "Gold Cost" : "";
}
}
namespace D2RAccess::AutoDetail {
bool Enabled() { return autoDetail; }
}
namespace D2RAccess::KeyboardMenu {
bool Active() { return keyboard; }
}
namespace D2RAccess::NativeSkillBar { std::string SkillName(int) { return {}; } }
namespace D2RAccess::MercenaryStats {
std::vector<Skill> Skills(uintptr_t) { return {}; }
// An icon's text as the engine keeps a tooltip: bottom-up.
Skill SkillOf(uintptr_t icon) {
	Skill skill;
	if (icon == Jab) skill.text = "Mana Cost: 3\nattacks with a series of rapid thrusts\nJab";
	if (icon == Aura) skill.text = "Radius: 14.6 yards\nDefiance";
	return skill;
}
}
namespace D2RAccess::NativeUi {
Type Classify(uintptr_t widget) {
	if (widget == Jab || widget == Aura) return {"HirelingSkillIconWidget", UiText::Kind::Focusable};
	if (widget == Price || widget == Gold) return {"FocusableWidget", UiText::Kind::Focusable};
	return {"ListItemWidget", widget >= 11 && widget <= 19 ? UiText::Kind::ListItem : UiText::Kind::Other};
}
uintptr_t Parent(uintptr_t widget) { return widget == Price ? PriceBox : widget == Jab || widget == Aura || widget == PriceBox ? 13 : 0; }
}
namespace D2RAccess::NativeTooltips {
void Track(uintptr_t panel) { trackedPanel = panel; }
Snapshot Latest(uint64_t) { return tooltip; }
}
namespace D2RAccess::InputDirection {
StickPush PeekRightStickPush() { return pending; }
StickPush PeekLeftStickPush() { return pendingLeft; }
bool LineStepPending() { return lineStep && pending != StickPush::None; }
bool FaceButtonHeld(FaceButton button) { return button == FaceButton::Y && holdingY; }
bool TakeLeftStickPushIf(StickPush wanted, const char*) {
	if (wanted == StickPush::None || pendingLeft != wanted) return false;
	pendingLeft = StickPush::None;
	return true;
}
bool TakeRightStickPushIf(StickPush wanted, const char*) {
	if (wanted != pending) return false;
	pending = StickPush::None;
	return true;
}
}
// The character panel's own cursor rule, which this reader shares.
namespace D2RAccess::CharacterStatsReader {
int Advance(int cursor, InputDirection::StickPush push, int count) {
	if (count <= 0) return -1;
	switch (push) {
	case InputDirection::StickPush::Up:    return std::max(cursor - 1, 0);
	case InputDirection::StickPush::Down:  return std::min(cursor + 1, count - 1);
	case InputDirection::StickPush::Left:  return 0;
	case InputDirection::StickPush::Right: return count - 1;
	default:                               return cursor;
	}
}
}
namespace D2RAccess::PadFilter {
int arms = 0;
void ArmLeftStickWithY(uint64_t) { ++arms; }
}
namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool) { speech.emplace_back(text); }
}

int main() {
	using InputDirection::StickPush;
	Strings::Select(Strings::Language::enUS);
	const NativeUi::Focus row{10, 11};
	Check(!Tick({20, 21}), "another menu is not the hire list");
	Check(Tick(row) && trackedPanel == 30, "the hire list keeps tooltip capture on");
	Check(Push(row, StickPush::Down) && speech == std::vector<std::string>{Strings::Current().detailNone},
		"before the tooltip arrives there is nothing more to read");
	// Tooltip bodies are stored bottom-up; Read gives the screen's order.
	tooltip = {"Defense: 25\nLife: 180\nKundri", "", 10, 11};
	Tick(row);
	Check(Push(row, StickPush::Down) && speech == std::vector<std::string>{"Kundri"}, "down starts at the first line");
	Check(Push(row, StickPush::Down) && speech == std::vector<std::string>{"Life: 180"}, "and walks on");
	Check(Push(row, StickPush::Right) && speech == std::vector<std::string>{"Defense: 25"}, "right is the last line");
	Check(Push(row, StickPush::Down) && speech == std::vector<std::string>{"Defense: 25"}, "past the end the last is read again");
	Check(Push(row, StickPush::Up) && speech == std::vector<std::string>{"Life: 180"}, "up goes back");
	Check(Push(row, StickPush::Left) && speech == std::vector<std::string>{"Kundri"}, "left is the first line");
	// Another mercenary: its own tooltip, from its first line again.
	const NativeUi::Focus next{10, 12};
	Check(Push(next, StickPush::Down) && speech == std::vector<std::string>{Strings::Current().detailNone},
		"the last mercenary's tooltip is never read for the next one");
	tooltip = {"Life: 210\nGaile", "", 10, 12};
	Tick(next);
	Check(Push(next, StickPush::Down) && speech == std::vector<std::string>{"Gaile"}, "the next mercenary's lines, from the top");
	// Holding Y draws one tooltip per skill; all of them are the lines, and
	// the left stick walks them while Y is held, as the right stick does.
	tooltip.extras = {"Mana Cost: 3\nFire Arrow", "Inner Sight"}; Tick(next);
	Check(Push(next, StickPush::Right) && speech == std::vector<std::string>{"Inner Sight"}, "every skill drawn is in the lines, the last one last");
	speech.clear(); pendingLeft = StickPush::Left; Tick(next);
	Check(speech == std::vector<std::string>{"Gaile"}, "the left stick walks them too, left to the first line");
	speech.clear(); pendingLeft = StickPush::Down; Tick(next);
	Check(speech == std::vector<std::string>{"Life: 210"}, "and down to the next");
	Check(PadFilter::arms > 0, "the hire list arms the left stick for a held Y");
	// The right stick is not the hire list's: without Y there is nothing past
	// the row, which the menu reader already says.
	speech.clear(); pending = StickPush::Down; Tick(next);
	Check(pending == StickPush::Down && speech.empty(), "the right stick is left alone in the hire list");
	pending = StickPush::None;
	Check(!Tick({20, 21}), "leaving the hire list lets the stick go");

	// Keyboard mode: the cursor's row is the focus; Shift+arrows walk a line,
	// Shift+PageUp reads the highlighted mercenary and Shift+PageDown the next record.
	keyboard = true;
	lineStep = true;
	const NativeUi::Focus third{10, 13};
	tooltip = {"", "", 0, 0};
	Tick(third);
	Check(Push(third, StickPush::Left) && speech == std::vector<std::string>{"Aliza"},
		"with no tooltip drawn the row's own name is read");
	tooltip = {"Life: 180\nAliza", "", 10, 13};
	tooltip.extras = {"Life: 120\nKundri"};
	Tick(third);
	Check(Push(third, StickPush::Left) && speech == std::vector<std::string>{"Aliza. Life: 180"}, "PageUp reads the highlighted one in full");
	pending = StickPush::Down;
	speech.clear();
	Check(Tick(third) && speech == std::vector<std::string>{"Life: 180"}, "a line step walks its lines");
	Check(Push(third, StickPush::Right) && speech == std::vector<std::string>{"Kundri. Life: 120"}, "PageDown reads the record drawn beside it");
	Check(Push(third, StickPush::Right) && speech == std::vector<std::string>{"Kundri. Life: 120"}, "and stops at the last");
	pending = StickPush::Down;
	speech.clear();
	Check(Tick(third) && speech == std::vector<std::string>{"Life: 120"}, "the line step follows the record read");
	tooltip.extras.clear();
	Tick({10, 14});
	tooltip = {"Life: 90\nGaile", "", 10, 14};
	Tick({10, 14});
	Check(Push({10, 14}, StickPush::Right) && speech == std::vector<std::string>{Strings::Current().detailNone},
		"with nothing drawn beside it there is nothing to compare");
	// The keyboard's cursor on a skill icon of a row: the icon is called by its
	// skill, and a line step walks that skill's tooltip and no other's.
	const NativeUi::Focus jab{10, Jab}, aura{10, Aura}, price{10, Price}, gold{10, Gold};
	tooltip = {"", "", 0, 0};
	Check(HireStats::Said(Jab, 10) == "Jab", "an icon is called by its skill");
	autoDetail = true;
	Check(HireStats::Said(Jab, 10) == "Jab. attacks with a series of rapid thrusts. Mana Cost: 3", "and by its whole tooltip when details are read unasked");
	autoDetail = false;
	Check(HireStats::Said(13, 10).empty(), "a row is left to the menu reader");
	Check(HireStats::Said(Jab, 20).empty(), "an icon of another panel is not the hire list's");
	Check(Tick(jab), "an icon under the cursor is the hire list's");
	pending = StickPush::Left; speech.clear();
	Check(Tick(jab) && speech == std::vector<std::string>{"Jab"}, "the first line of an icon is its skill");
	pending = StickPush::Down; speech.clear();
	Check(Tick(jab) && speech == std::vector<std::string>{"attacks with a series of rapid thrusts"}, "a line step walks the icon's own tooltip");
	pending = StickPush::Right; speech.clear();
	Check(Tick(jab) && speech == std::vector<std::string>{"Mana Cost: 3"}, "to its last line");
	pending = StickPush::Right; speech.clear();
	Check(Tick(aura) && speech == std::vector<std::string>{"Radius: 14.6 yards"}, "the next icon has its own lines, not the last one's");
	Check(HireStats::Said(Price, 10) == "Gold Cost. 717", "a price is the game's words for it and the row's amount");
	Check(HireStats::Said(Gold, 10) == "Your current gold. 12,345", "the gold line is the game's words and the amount");
	pending = StickPush::Left; speech.clear();
	Check(Tick(price) && speech == std::vector<std::string>{"Gold Cost. 717"}, "a line step on a price reads it");
	Check(Tick(gold), "the gold line under the cursor is the hire list's");
	keyboard = false;
	Check(!Tick(jab) && !Tick(gold), "on a controller the icons and the gold lines are not this reader's");
	Passed();
}
