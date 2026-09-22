#include "hire_stats.h"

#include "async_log.h"
#include "character_stats_reader.h"
#include "input_direction.h"
#include "native_tooltips.h"
#include "native_widgets.h"
#include "pad_filter.h"
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
std::vector<std::string> g_lines; // every tooltip drawn for it, in the order the screen draws them
int g_cursor = -1;               // -1: nothing read yet for this row
}

bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus) {
	if (!focus.widget || NativeWidgets::ReadName(focus.panel) != Panel
		|| NativeUi::Classify(focus.widget).kind != UiText::Kind::ListItem) {
		Reset();
		return false;
	}
	NativeTooltips::Track(NativeWidgets::FindPanel("TooltipsPanel"));
	PadFilter::ArmLeftStickWithY(now);
	if (focus.widget != g_row) {
		g_row = focus.widget;
		g_lines.clear();
		g_cursor = -1;
	}
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
	// The right stick walks them; while Y is held the left stick does the same,
	// since Y and the right stick share the right thumb (the user, 2026-09-22).
	InputDirection::StickPush push = InputDirection::StickPush::None;
	for (const auto wanted : {InputDirection::StickPush::Up, InputDirection::StickPush::Down,
	                          InputDirection::StickPush::Left, InputDirection::StickPush::Right}) {
		if (InputDirection::TakeRightStickPushIf(wanted, "hire") || InputDirection::TakeLeftStickPushIf(wanted, "hire")) {
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

void Reset() {
	g_row = 0;
	g_lines.clear();
	g_cursor = -1;
}

}
