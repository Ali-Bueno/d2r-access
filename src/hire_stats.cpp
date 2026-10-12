#include "hire_stats.h"

#include "async_log.h"
#include "auto_detail.h"
#include "character_stats_reader.h"
#include "input_direction.h"
#include "keyboard_menu.h"
#include "localization.h"
#include "mercenary_stats.h"
#include "native_tooltips.h"
#include "native_widgets.h"
#include "pad_filter.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "tooltip_text.h"
#include "ui_text.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::HireStats {
namespace {
constexpr std::string_view Panel = "HireMenuPanel";
constexpr std::string_view SkillIcon = "HirelingSkillIconWidget";
// The two gold lines (HireMenuPanelHD.json): the widget the pointer goes on,
// the game's string for its tooltip, and the text box that holds the amount.
constexpr std::string_view YourGold = "YourGoldTooltipWidget", GoldCost = "GoldCostTooltipWidget";
constexpr const char* YourGoldKey = "HireMenuPanelYourGoldIconTooltip";
constexpr const char* GoldCostKey = "HireMenuPanelMercenaryGoldCostTooltip";

enum class Part { None, Row, Skill, Gold };

uintptr_t g_row = 0;             // the list row the lines belong to
std::vector<std::string> g_lines; // every tooltip drawn for it, in the order the screen draws them
int g_cursor = -1;               // -1: nothing read yet for this row

// Keyboard mode (the user, 2026-09-30): the cursor's row is the focus, and the
// item keys read it. Record 0 is the highlighted mercenary -- the tooltip the
// game draws for the row if it draws one, else the row's own texts, then each
// skill as its icon's tooltip reads -- and every other tooltip record the game
// draws beside it follows. What the game draws for a row under the pointer is
// NOT measured; the [hire] lines in the log say.
std::vector<std::vector<std::string>> g_tip; // the row's tooltip records, kept once drawn
std::vector<std::vector<std::string>> g_records;
size_t g_record = 0, g_line = 0;
bool g_yHeld = false;

std::string Joined(const std::vector<std::string>& lines) {
	std::string text;
	for (const auto& line : lines) {
		if (!text.empty()) text += text.ends_with('.') ? " " : ". ";
		text += line;
	}
	return text;
}

// "Skills: Exploding Arrow, Fire Arrow, Inner Sight", read off a row's own icons;
// with autodetail each skill's whole description.
std::string SkillSummary(uintptr_t row) {
	std::string names;
	for (const auto& skill : MercenaryStats::Skills(row)) {
		const auto lines = TooltipText::Read(skill.text).lines;
		if (lines.empty()) continue;
		if (!names.empty()) names += AutoDetail::Enabled() ? ". " : ", ";
		names += AutoDetail::Enabled() ? Joined(lines) : lines.front();
	}
	if (names.empty()) return {};
	char text[1200]{};
	std::snprintf(text, sizeof(text), Strings::Current().mercenarySkills, names.c_str());
	return text;
}

// A row is the controller's as well; the icons and the gold lines are places only of the keyboard's cursor.
Part PartOf(NativeUi::Focus focus) {
	if (!focus.widget || NativeWidgets::ReadName(focus.panel) != Panel) return Part::None;
	const auto type = NativeUi::Classify(focus.widget);
	if (type.kind == UiText::Kind::ListItem) return Part::Row;
	if (!KeyboardMenu::Active()) return Part::None;
	if (type.name == SkillIcon) return Part::Skill;
	const std::string name = NativeWidgets::ReadName(focus.widget);
	return name == YourGold || name == GoldCost ? Part::Gold : Part::None;
}

std::vector<std::string> SkillLines(uintptr_t icon) { return TooltipText::Read(MercenaryStats::SkillOf(icon).text).lines; }

// "Your current gold. 12,345" and "Gold Cost. 717": the tooltip's words, then the number drawn beside it.
std::string GoldLine(uintptr_t widget, uintptr_t panel) {
	const bool mine = NativeWidgets::ReadName(widget) == YourGold;
	std::string text = UiText::Clean(Localization::Text(mine ? YourGoldKey : GoldCostKey));
	// The price is drawn beside its tooltip's widget, inside the same container.
	const uintptr_t amount = mine ? NativeWidgets::FindChild(panel, "GoldText")
		: NativeWidgets::FindChild(NativeUi::Parent(widget), "HireCost");
	if (const std::string number = UiText::Clean(NativeWidgets::ReadText(amount)); !number.empty()) {
		if (!text.empty()) text += ". ";
		text += number;
	}
	return text;
}

// Shift or Ctrl with an arrow: a line of the record read last. Shift+PageUp:
// the highlighted mercenary in full; Shift+PageDown: the next record the game
// draws beside it, stopping at the last -- the keys an item is compared with.
bool KeyboardTick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus, Part part) {
	using InputDirection::StickPush;
	std::vector<std::vector<std::string>> records(1);
	if (part == Part::Skill) {
		// One icon: its own tooltip is the record, a line to a line.
		records[0] = SkillLines(focus.widget);
	} else if (part == Part::Gold) {
		if (auto line = GoldLine(focus.widget, focus.panel); !line.empty()) records[0].push_back(std::move(line));
	} else {
		if (const auto tooltip = NativeTooltips::Latest(now); tooltip.panel == focus.panel && tooltip.widget == focus.widget) {
			std::vector<std::vector<std::string>> tip{TooltipText::Read(tooltip.body).lines};
			for (const auto& extra : tooltip.extras)
				if (auto lines = TooltipText::Read(extra).lines; !lines.empty()) tip.push_back(std::move(lines));
			if (!tip[0].empty() || tip.size() > 1) g_tip = std::move(tip);
		}
		if (!g_tip.empty()) records = g_tip;
		if (records[0].empty())
			for (const char* name : {"HireName", "HireLevel", "HireCost"})
				if (auto text = UiText::Clean(NativeWidgets::ReadText(NativeWidgets::FindChild(focus.widget, name))); !text.empty())
					records[0].push_back(std::move(text));
		for (const auto& skill : MercenaryStats::Skills(focus.widget))
			for (auto& line : TooltipText::Read(skill.text).lines) records[0].push_back(std::move(line));
	}
	if (records != g_records) {
		for (size_t i = 0; i < records.size(); ++i)
			AsyncLog::InfoF(context, "[hire] record %zu, %zu lines: %s", i, records[i].size(), Joined(records[i]).c_str());
		g_records = std::move(records);
		if (g_record >= g_records.size()) g_record = 0;
		g_line = 0;
	}
	if (InputDirection::LineStepPending()) {
		const auto push = InputDirection::PeekRightStickPush();
		if (InputDirection::TakeRightStickPushIf(push, "hire line")) {
			const auto& lines = g_records[g_record];
			std::string text = Strings::Current().detailNone;
			if (!lines.empty()) {
				const size_t last = lines.size() - 1;
				if (g_line > last) g_line = last;
				// At either end the step reads the same line again rather than nothing.
				if (push == StickPush::Up && g_line > 0) --g_line;
				else if (push == StickPush::Down && g_line < last) ++g_line;
				else if (push == StickPush::Left) g_line = 0;
				else if (push == StickPush::Right) g_line = last;
				text = lines[g_line];
			}
			AsyncLog::InfoF(context, "[hire] record %zu line %zu -> \"%s\"", g_record, g_line, text.c_str());
			ScreenReader::Speak(text, true);
		}
	}
	const auto push = InputDirection::PeekLeftStickPush();
	if ((push == StickPush::Left || push == StickPush::Right) && InputDirection::TakeLeftStickPushIf(push, "hire compare")) {
		std::string text;
		if (push == StickPush::Left) g_record = 0;
		else if (g_record + 1 < g_records.size()) ++g_record;
		else if (g_records.size() == 1) g_record = g_records.size(); // nothing drawn to compare against
		if (g_record < g_records.size()) text = Joined(g_records[g_record]);
		else g_record = 0;
		if (text.empty()) text = Strings::Current().detailNone;
		g_line = 0;
		AsyncLog::InfoF(context, "[hire] compare record %zu of %zu -> \"%s\"", g_record, g_records.size(), text.c_str());
		ScreenReader::Speak(text, true);
	}
	return true;
}
}

bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus) {
	const Part part = PartOf(focus);
	if (part == Part::None) {
		Reset();
		return false;
	}
	NativeTooltips::Track(NativeWidgets::FindPanel("TooltipsPanel"));
	if (part == Part::Row) PadFilter::ArmLeftStickWithY(now);
	if (focus.widget != g_row) {
		g_row = focus.widget;
		g_lines.clear();
		g_cursor = -1;
		g_tip.clear();
		g_records.clear();
		g_record = g_line = 0;
	}
	if (KeyboardMenu::Active()) return KeyboardTick(context, now, focus, part);
	// Y shows the skills on screen and nothing says them (a tester: vital to choose
	// whom to hire), so the press names them at once; the left stick then walks them.
	const bool y = InputDirection::FaceButtonHeld(InputDirection::FaceButton::Y);
	if (y && !g_yHeld && part == Part::Row)
		if (const auto summary = SkillSummary(focus.widget); !summary.empty()) {
			AsyncLog::InfoF(context, "[hire] Y -> \"%s\"", summary.c_str());
			ScreenReader::Speak(summary, true);
		}
	g_yHeld = y;
	// MEASURED 2026-09-22: the hire list draws no tooltip for a highlighted
	// row; holding Y draws the mercenary's skills (ui-controller.json "Show
	// Mercenary Skills"), one tooltip per skill -- Exploding Arrow first, then
	// Fire Arrow and Inner Sight. All of them are the lines.
	if (const auto tooltip = NativeTooltips::Latest(now); tooltip.panel == focus.panel && tooltip.widget == focus.widget) {
		auto lines = TooltipText::Read(tooltip.body).lines;
		for (const auto& extra : tooltip.extras)
			for (auto& line : TooltipText::Read(extra).lines) lines.push_back(std::move(line));
		if (lines != g_lines) {
			std::string joined;
			for (const auto& line : lines) joined += (joined.empty() ? "" : " | ") + line;
			AsyncLog::InfoF(context, "[hire] %zu lines: %s", lines.size(), joined.c_str());
			g_lines = std::move(lines);
			g_cursor = -1;
		}
	}
	// The left stick walks them while Y is held -- Y and the right stick share
	// the right thumb. The right stick is not used here: without Y there is
	// nothing past the row, which the menu reader already says (the user,
	// 2026-09-22: "take the buffer out of mercenaries ... the skills reader is
	// very necessary though").
	InputDirection::StickPush push = InputDirection::StickPush::None;
	for (const auto wanted : {InputDirection::StickPush::Up, InputDirection::StickPush::Down,
	                          InputDirection::StickPush::Left, InputDirection::StickPush::Right}) {
		if (InputDirection::TakeLeftStickPushIf(wanted, "hire skills")) {
			push = wanted;
			break;
		}
	}
	if (push == InputDirection::StickPush::None) return true;
	g_cursor = CharacterStatsReader::Advance(g_cursor, push, static_cast<int>(g_lines.size()));
	const std::string& text = g_cursor < 0 ? Strings::Current().detailNone : g_lines[static_cast<size_t>(g_cursor)];
	AsyncLog::InfoF(context, "[hire] %d -> \"%s\"", g_cursor, text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

std::string Said(uintptr_t widget, uintptr_t panel) {
	if (!widget || NativeWidgets::ReadName(panel) != Panel) return {};
	if (NativeUi::Classify(widget).name == SkillIcon) {
		const auto lines = SkillLines(widget);
		if (lines.empty()) return {};
		return AutoDetail::Enabled() ? Joined(lines) : lines.front();
	}
	const std::string name = NativeWidgets::ReadName(widget);
	return name == YourGold || name == GoldCost ? GoldLine(widget, panel) : std::string{};
}

void Reset() {
	g_row = 0;
	g_lines.clear();
	g_cursor = -1;
	g_tip.clear();
	g_records.clear();
	g_record = g_line = 0;
}

}
