#include "hire_stats.h"
#include "character_stats_reader.h"
#include "input_direction.h"
#include "native_tooltips.h"
#include "native_widgets.h"
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
InputDirection::StickPush pending = InputDirection::StickPush::None;
std::vector<std::string> speech;
const D2RL::PluginContext context{};
bool Tick(NativeUi::Focus focus) { return HireStats::Tick(&context, 1000, focus); }
bool Push(NativeUi::Focus focus, InputDirection::StickPush push) { pending = push; speech.clear(); return Tick(focus); }
}

namespace D2RAccess::NativeWidgets {
std::string ReadName(uintptr_t id) { return id == 10 ? "HireMenuPanel" : id == 30 ? "TooltipsPanel" : "OtherPanel"; }
uintptr_t FindPanel(std::string_view name) { return name == "TooltipsPanel" ? 30 : 0; }
}
namespace D2RAccess::NativeUi {
Type Classify(uintptr_t widget) { return {"ListItemWidget", widget >= 11 && widget <= 19 ? UiText::Kind::ListItem : UiText::Kind::Other}; }
}
namespace D2RAccess::NativeTooltips {
void Track(uintptr_t panel) { trackedPanel = panel; }
Snapshot Latest(uint64_t) { return tooltip; }
}
namespace D2RAccess::InputDirection {
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
	speech.clear(); tooltip.extras = {"Life: 150\nFlavie"}; Tick(next);
	Check(speech == std::vector<std::string>{"Flavie. Life: 150"}, "holding compare reads the current mercenary");
	speech.clear(); Tick(next);
	Check(speech.empty(), "once per hold");
	pending = StickPush::None;
	Check(!Tick({20, 21}), "leaving the hire list lets the stick go");
	Passed();
}
