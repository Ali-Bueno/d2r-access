#include "hire_stats.h"

#include "async_log.h"
#include "character_stats_reader.h"
#include "input_direction.h"
#include "native_tooltips.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "tooltip_text.h"

#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::HireStats {
namespace {
constexpr std::string_view Panel = "HireMenuPanel";

uintptr_t g_row = 0;             // the list row the lines belong to
std::vector<std::string> g_lines; // its tooltip, in the order the screen draws it
int g_cursor = -1;               // -1: nothing read yet for this row
std::string g_comparison;        // the current mercenary, drawn beside it while Y is held
}

bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus) {
	if (!focus.widget || NativeWidgets::ReadName(focus.panel) != Panel
		|| NativeUi::Classify(focus.widget).kind != UiText::Kind::ListItem) {
		Reset();
		return false;
	}
	NativeTooltips::Track(NativeWidgets::FindPanel("TooltipsPanel"));
	if (focus.widget != g_row) {
		g_row = focus.widget;
		g_lines.clear();
		g_cursor = -1;
		g_comparison.clear();
	}
	if (const auto tooltip = NativeTooltips::Latest(now); tooltip.panel == focus.panel && tooltip.widget == focus.widget) {
		auto lines = TooltipText::Read(tooltip.body).lines;
		if (lines != g_lines) {
			std::string joined;
			for (const auto& line : lines) joined += (joined.empty() ? "" : " | ") + line;
			AsyncLog::InfoF(context, "[hire] %zu lines: %s", lines.size(), joined.c_str());
			g_lines = std::move(lines);
			g_cursor = -1;
		}
		// Holding the compare button draws the current mercenary beside it.
		auto comparison = TooltipText::Comparison(tooltip.extras);
		if (comparison != g_comparison && !comparison.empty())
			AsyncLog::InfoF(context, "[hire] compare drawn -> \"%s\"", comparison.c_str());
		g_comparison = std::move(comparison);
	}
	// Compare mode while Y is held, as in the inventory: left reads the
	// highlighted mercenary in full, right the current one. Up and down still
	// walk the lines.
	if (InputDirection::FaceButtonHeld(InputDirection::FaceButton::Y)) {
		const auto push = InputDirection::PeekRightStickPush();
		if (push == InputDirection::StickPush::Left || push == InputDirection::StickPush::Right) {
			(void)InputDirection::TakeRightStickPushIf(push, "hire compare");
			std::string text;
			if (push == InputDirection::StickPush::Left)
				for (const auto& line : g_lines) text += (text.empty() ? "" : ". ") + line;
			else text = g_comparison;
			if (text.empty()) text = Strings::Current().detailNone;
			AsyncLog::InfoF(context, "[hire] compare %s -> \"%s\"",
				push == InputDirection::StickPush::Left ? "highlighted" : "current", text.c_str());
			ScreenReader::Speak(text, true);
			return true;
		}
	}
	InputDirection::StickPush push = InputDirection::StickPush::None;
	for (const auto wanted : {InputDirection::StickPush::Up, InputDirection::StickPush::Down,
	                          InputDirection::StickPush::Left, InputDirection::StickPush::Right}) {
		if (InputDirection::TakeRightStickPushIf(wanted, "hire")) { push = wanted; break; }
	}
	if (push == InputDirection::StickPush::None) return true;
	g_cursor = CharacterStatsReader::Advance(g_cursor, push, static_cast<int>(g_lines.size()));
	const std::string& text = g_cursor < 0 ? Strings::Current().detailNone : g_lines[static_cast<size_t>(g_cursor)];
	AsyncLog::InfoF(context, "[hire] %d -> \"%s\"", g_cursor, text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

void Reset() {
	g_row = 0;
	g_lines.clear();
	g_cursor = -1;
	g_comparison.clear();
}

}
